#if CUDA_AVAILABLE == 1

#include "./LocalCUDAEngine.cuh"
#include "LocalEngine.h"
#include <numeric>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <iostream>


void LocalCUDAEngine::initializeCuda() {
    initCudaLattice();

    size_t bytes = this->n_vals * sizeof(double);
    cudaMalloc(&this->g_src, bytes);
    cudaMalloc(&this->f_src, bytes);
    cudaMalloc(&this->g_mid, bytes);
    cudaMalloc(&this->f_mid, bytes);
    cudaMalloc(&this->g_dst, bytes);
    cudaMalloc(&this->f_dst, bytes);

    cudaMalloc(&this->d_temperatures, this->cells * sizeof(double));
    cudaMalloc(&this->d_max_temp,     sizeof(double));
    cudaMalloc(&this->d_min_temp,     sizeof(double));
    cudaMalloc(&this->d_temp_sum,     sizeof(double));

    cudaMalloc(&this->d_isRad, this->cells * sizeof(bool));

    this->d_boundary_indices = nullptr;
    this->d_rad_flux = nullptr;
    this->d_t4 = nullptr;
    this->d_cellToBoundary = nullptr;
    this->d_vfStart = nullptr;
    this->d_vfSourceCSR = nullptr;
    this->d_vfFactorCSR = nullptr;
    this->d_total_radiation = nullptr;
    this->n_boundary = 0;
    this->n_viewFactors = 0;

    cudaMallocHost(&this->h_pinned_temps, this->cells * sizeof(double));
}

// Grid upload / download

void LocalCUDAEngine::uploadInitialGrid() {
    auto state = this->getState();
    const Grid& grid = state->grid;
    cudaMemcpy(this->g_src, grid.g.data(), this->n_vals * sizeof(double), cudaMemcpyHostToDevice);
    cudaMemcpy(this->f_src, grid.f.data(), this->n_vals * sizeof(double), cudaMemcpyHostToDevice);
}

void LocalCUDAEngine::downloadFullGrid(Grid& grid) const {
    cudaMemcpy(grid.g.data(), this->g_src, this->n_vals * sizeof(double), cudaMemcpyDeviceToHost);
    cudaMemcpy(grid.f.data(), this->f_src, this->n_vals * sizeof(double), cudaMemcpyDeviceToHost);
}


LocalCUDAEngine::LocalCUDAEngine(int w, int h, bool constantHeatSource)
    : SimulationEngine(w, h), n_vals(9 * cells) {
    this->initializeCuda();

    auto initialState = std::make_unique<SimulationState>();
    initialState->width = w;
    initialState->height = h;
    auto cells = initialState->cells = w * h;
    initialState->grid = Grid(cells);
    initialState->current_step = 0;
    initialState->isConstantHeatSource = constantHeatSource;

    // Shared physics init (same as LocalEngine)
    initPhysics(*initialState, w, h);

    // Temperatures and initial grid
    initialState->temperatures.resize(cells, ROOM_TEMP);
    for (int idx : initialState->heatSources) {
        initialState->temperatures[idx] = ROOM_TEMP;
    }

    double tempAvgLocal = 0.0;
    for (int i = 0; i < cells; i++) {
        for (int d = 0; d < 9; ++d) {
            initialState->grid.g[d * cells + i] = weights[d] * initialState->temperatures[i];
            initialState->grid.f[d * cells + i] = weights[d] * 1.0;
        }
        tempAvgLocal += initialState->temperatures[i];
    }
    initialState->TempAvg = tempAvgLocal / cells;

    this->history = std::make_unique<SimulationHistory>();
    this->history->time_history.push_back(initialState->current_step);
    this->history->max_temp_history.push_back(ROOM_TEMP);
    this->history->min_temp_history.push_back(ROOM_TEMP);
    this->history->temperature_history.push_back(initialState->temperatures);
    this->history->convectionOutput.push_back(0.0);
    this->history->radiationOutput.push_back(0.0);

    // Allocate persistent GPU radiation buffers
    this->n_boundary = initialState->boundaryCells.size();
    this->n_viewFactors = initialState->viewFactors.size();

    cudaMalloc(&this->d_t4, cells * sizeof(double));
    cudaMalloc(&this->d_cellToBoundary, cells * sizeof(int));
    cudaMalloc(&this->d_total_radiation, sizeof(double));

    cudaMemcpy(this->d_cellToBoundary, initialState->cellToBoundary.data(),
               cells * sizeof(int), cudaMemcpyHostToDevice);

    if (this->n_boundary > 0) {
        cudaMalloc(&this->d_boundary_indices, this->n_boundary * sizeof(int));
        cudaMalloc(&this->d_rad_flux, this->n_boundary * sizeof(double));
        cudaMemcpy(this->d_boundary_indices, initialState->boundaryCells.data(),
                   this->n_boundary * sizeof(int), cudaMemcpyHostToDevice);
    }

    // Upload CSR view factor data (grouped by boundary cell, zero-atomics GPU kernel)
    if (this->n_boundary > 0) {
        cudaMalloc(&this->d_vfStart, (this->n_boundary + 1) * sizeof(int));
        cudaMemcpy(this->d_vfStart, initialState->boundaryVFStart.data(),
                   (this->n_boundary + 1) * sizeof(int), cudaMemcpyHostToDevice);
    }
    if (this->n_viewFactors > 0) {
        cudaMalloc(&this->d_vfSourceCSR, this->n_viewFactors * sizeof(int));
        cudaMalloc(&this->d_vfFactorCSR, this->n_viewFactors * sizeof(double));
        cudaMemcpy(this->d_vfSourceCSR, initialState->vfSourceCSR.data(),
                   this->n_viewFactors * sizeof(int), cudaMemcpyHostToDevice);
        cudaMemcpy(this->d_vfFactorCSR, initialState->vfFactorCSR.data(),
                   this->n_viewFactors * sizeof(double), cudaMemcpyHostToDevice);
    }

    // Thread must be created before uploadInitialGrid (getState needs it)
    this->thread = std::make_unique<ReusableThread>(std::move(initialState));

    this->uploadInitialGrid();

    cudaMemcpy(this->d_isRad, this->getState()->isRad.data(),
               cells * sizeof(bool), cudaMemcpyHostToDevice);

    // Calculate initial temperatures on GPU for launchRadiationFlux
    this->launchReduce();
}

LocalCUDAEngine::LocalCUDAEngine(int w, int h, bool constantHeatSource,
    std::unique_ptr<SimulationState> initialState,
    std::unique_ptr<SimulationHistory> initialHistory)
    : SimulationEngine(w, h), n_vals(9 * cells) {
    this->initializeCuda();
    this->history = std::move(initialHistory);

    this->thread = std::make_unique<ReusableThread>(std::move(initialState));

    auto* state = this->getState();
    this->n_boundary = state->boundaryCells.size();
    this->n_viewFactors = state->viewFactors.size();

    cudaMalloc(&this->d_t4, cells * sizeof(double));
    cudaMalloc(&this->d_cellToBoundary, cells * sizeof(int));
    cudaMalloc(&this->d_total_radiation, sizeof(double));

    cudaMemcpy(this->d_cellToBoundary, state->cellToBoundary.data(),
               cells * sizeof(int), cudaMemcpyHostToDevice);

    if (this->n_boundary > 0) {
        cudaMalloc(&this->d_boundary_indices, this->n_boundary * sizeof(int));
        cudaMalloc(&this->d_rad_flux, this->n_boundary * sizeof(double));
        cudaMemcpy(this->d_boundary_indices, state->boundaryCells.data(),
                   this->n_boundary * sizeof(int), cudaMemcpyHostToDevice);
    }

    if (this->n_boundary > 0) {
        cudaMalloc(&this->d_vfStart, (this->n_boundary + 1) * sizeof(int));
        cudaMemcpy(this->d_vfStart, state->boundaryVFStart.data(),
                   (this->n_boundary + 1) * sizeof(int), cudaMemcpyHostToDevice);
    }
    if (this->n_viewFactors > 0) {
        cudaMalloc(&this->d_vfSourceCSR, this->n_viewFactors * sizeof(int));
        cudaMalloc(&this->d_vfFactorCSR, this->n_viewFactors * sizeof(double));
        cudaMemcpy(this->d_vfSourceCSR, state->vfSourceCSR.data(),
                   this->n_viewFactors * sizeof(int), cudaMemcpyHostToDevice);
        cudaMemcpy(this->d_vfFactorCSR, state->vfFactorCSR.data(),
                   this->n_viewFactors * sizeof(double), cudaMemcpyHostToDevice);
    }

    this->uploadInitialGrid();
    cudaMemcpy(this->d_isRad, state->isRad.data(),
               cells * sizeof(bool), cudaMemcpyHostToDevice);
    this->launchReduce();
}

LocalCUDAEngine::~LocalCUDAEngine() {
    cudaFree(this->g_src); cudaFree(this->f_src);
    cudaFree(this->g_mid); cudaFree(this->f_mid);
    cudaFree(this->g_dst); cudaFree(this->f_dst);
    cudaFree(this->d_temperatures);
    cudaFree(this->d_max_temp);
    cudaFree(this->d_min_temp);
    cudaFree(this->d_temp_sum);
    cudaFree(this->d_isRad);
    if (this->d_boundary_indices) cudaFree(this->d_boundary_indices);
    if (this->d_rad_flux) cudaFree(this->d_rad_flux);
    if (this->d_t4) cudaFree(this->d_t4);
    if (this->d_cellToBoundary) cudaFree(this->d_cellToBoundary);
    if (this->d_vfStart) cudaFree(this->d_vfStart);
    if (this->d_vfSourceCSR) cudaFree(this->d_vfSourceCSR);
    if (this->d_vfFactorCSR) cudaFree(this->d_vfFactorCSR);
    if (this->d_total_radiation) cudaFree(this->d_total_radiation);
    cudaFreeHost(this->h_pinned_temps);
}



void LocalCUDAEngine::collision(const double tauT, const double TempAvg,
                                const double tauF, const double lattice_buoyancy) {
    dim3 block(16, 16);
    dim3 grid((this->width  + block.x - 1) / block.x,
              (this->height + block.y - 1) / block.y);

    collisionKernel<<<grid, block>>>(this->height, this->width,
                                     tauT, TempAvg, tauF, lattice_buoyancy,
                                     this->g_mid, this->f_mid,
                                     this->g_src, this->f_src);

    cudaError_t err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        fprintf(stderr, "collisionKernel error: %s\n", cudaGetErrorString(err));
        exit(1);
    }
}

void LocalCUDAEngine::stream() {
    dim3 block(16, 16);
    dim3 grid((this->width  + block.x - 1) / block.x,
              (this->height + block.y - 1) / block.y);

    streamKernel<<<grid, block>>>(this->height, this->width,
                                  this->g_dst, this->f_dst,
                                  this->g_mid, this->f_mid,
                                  this->d_isRad);

    cudaError_t err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        fprintf(stderr, "streamKernel error: %s\n", cudaGetErrorString(err));
        exit(1);
    }
}

void LocalCUDAEngine::launchReduce() {
    initScalarsKernel<<<1, 1>>>(this->d_max_temp, this->d_min_temp,
                                 this->d_temp_sum);
    reduceTemperatureKernel<<<256, 256>>>(
        this->cells, this->g_src,
        this->d_temperatures,
        this->d_max_temp, this->d_min_temp, this->d_temp_sum);
    cudaDeviceSynchronize();
}

void LocalCUDAEngine::launchHeatSourcePatch(const SimulationState& state) {
    if (!state.isConstantHeatSource || state.heatSources.empty()) return;

    static bool uploaded = false;
    static int* d_heatSources = nullptr;
    static int d_nHeatSources = 0;

    if (!uploaded) {
        uploaded = true;
        d_nHeatSources = state.heatSources.size();
        cudaMalloc(&d_heatSources, d_nHeatSources * sizeof(int));
        cudaMemcpy(d_heatSources, state.heatSources.data(),
                   d_nHeatSources * sizeof(int), cudaMemcpyHostToDevice);
    }

    int threads = std::min(256, (int)state.heatSources.size());
    int blocks = ((int)state.heatSources.size() + threads - 1) / threads;

    heatSourcePatchKernel<<<blocks, threads>>>(
        d_heatSources, (int)state.heatSources.size(),
        MAX_TEMP, this->d_temperatures,
        this->cells,
        this->g_src, this->f_src);

    cudaDeviceSynchronize();
}

void LocalCUDAEngine::launchRadiationFlux(const SimulationState& state) {
    if (this->n_viewFactors == 0 || this->n_boundary == 0) return;

    const double sb = state.lattice_stefan_boltzmann;

    // 1. Compute T^4 on GPU (element-wise, parallel)
    {
        int threads = 256;
        int blocks = (this->cells + threads - 1) / threads;
        computeT4Kernel<<<blocks, threads>>>(this->cells, this->d_temperatures, this->d_t4);
    }

    // 2. Zero out accumulators
    cudaMemset(this->d_rad_flux, 0, this->n_boundary * sizeof(double));
    cudaMemset(this->d_total_radiation, 0, sizeof(double));

    // 3. CSR radiation kernel — one thread per boundary cell, zero atomics on flux
    {
        int threads = 256;
        int blocks = (this->n_boundary + threads - 1) / threads;
        radiationFluxKernelCSR<<<blocks, threads>>>(
            this->n_boundary,
            this->d_boundary_indices,
            this->d_vfStart,
            this->d_vfSourceCSR,
            this->d_vfFactorCSR,
            this->d_t4,
            sb,
            this->d_rad_flux,
            this->d_total_radiation);
    }

    // 4. Apply accumulated flux to grid.g on GPU
    {
        int threads = std::min(256, this->n_boundary);
        int blocks = (this->n_boundary + threads - 1) / threads;
        applyRadiationFluxKernel<<<blocks, threads>>>(
            this->cells, this->d_boundary_indices, this->d_rad_flux,
            this->n_boundary, this->g_src);
    }

    cudaDeviceSynchronize();

    // 5. Download total radiation scalar for history
    double radiationThisStep;
    cudaMemcpy(&radiationThisStep, this->d_total_radiation, sizeof(double),
               cudaMemcpyDeviceToHost);
    history->radiationOutput.push_back(radiationThisStep);
}


void LocalCUDAEngine::stepFoward() {
    thread->submitTask([this](const SimulationState& previousState,
                               SimulationState& nextState) {

        if (previousState.current_step <
            (int)this->history->temperature_history.size() - 1) {
            nextState.current_step = previousState.current_step + 1;
            nextState.temperatures =
                this->history->temperature_history[nextState.current_step];
            if (this->getAutoPlayStatus()) {
                this->stepFoward();
            }
            return;
        }

        // 1. Radiation (CPU T^4 + view factors, GPU apply)
        this->launchRadiationFlux(previousState);

        // 2. Heat source patch (gradual heating on GPU)
        this->launchHeatSourcePatch(previousState);

        // 3-4. Collision -> Stream -> swap
        this->collision(previousState.tauT, previousState.TempAvg,
                        previousState.tauF, previousState.lattice_buoyancy);
        this->stream();
        this->swapDeviceGrids();

        // 5. Temperature reduction on GPU
        this->launchReduce();

        double current_max, current_min, temp_sum;
        cudaMemcpy(&current_max, this->d_max_temp, sizeof(double),
                   cudaMemcpyDeviceToHost);
        cudaMemcpy(&current_min, this->d_min_temp, sizeof(double),
                   cudaMemcpyDeviceToHost);
        cudaMemcpy(&temp_sum,   this->d_temp_sum, sizeof(double),
                   cudaMemcpyDeviceToHost);

        nextState.TempAvg = temp_sum / this->cells;

        // 6. Download temperatures (skipped in batch mode)
        if (!this->batchMode) {
            cudaMemcpy(this->h_pinned_temps, this->d_temperatures,
                       this->cells * sizeof(double), cudaMemcpyDeviceToHost);
            nextState.temperatures.resize(this->cells);
            std::memcpy(nextState.temperatures.data(), this->h_pinned_temps,
                        this->cells * sizeof(double));
        }

        // 7. History update
        double previousEnergy = previousState.TempAvg * this->cells;
        double currentEnergy = nextState.TempAvg * this->cells;
        double totalEnergyOutput = currentEnergy - previousEnergy;
        double convectionThisStep = totalEnergyOutput -
            (history->radiationOutput.empty() ? 0.0 : history->radiationOutput.back());
        history->convectionOutput.push_back(convectionThisStep);

        nextState.current_step = previousState.current_step + 1;
        this->history->time_history.push_back(nextState.current_step);
        this->history->max_temp_history.push_back(current_max);
        this->history->min_temp_history.push_back(current_min);
        if (!this->batchMode) {
            this->history->temperature_history.push_back(nextState.temperatures);
        }

        if (this->getAutoPlayStatus()) {
            this->stepFoward();
        }
    });
}

void LocalCUDAEngine::stepBack() {
    thread->submitTask([this](const SimulationState& previousState,
                               SimulationState& nextState) {
        if (previousState.current_step <= 0) return;
        nextState.current_step = previousState.current_step - 1;
        nextState.temperatures =
            this->history->temperature_history[nextState.current_step];
        nextState.grid = previousState.grid;
    });
}

void LocalCUDAEngine::seekTo(int step) {
    thread->submitTask([this, step](const SimulationState& previousState,
                                     SimulationState& nextState) {
        if (step < 0 || step >= (int)this->history->temperature_history.size())
            return;
        nextState.current_step = step;
        nextState.temperatures =
            this->history->temperature_history[nextState.current_step];
        nextState.grid = previousState.grid;
    });
}

double LocalCUDAEngine::getTotalEnergy() const {
    auto state = thread->getState();
    return std::accumulate(state->temperatures.begin(),
                           state->temperatures.end(), 0.0);
}


std::unique_ptr<LocalCUDAEngine> loadLocalCUDASimulation(const std::string& filepath) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return nullptr;
    }

    int w, h;
    in.read(reinterpret_cast<char*>(&w), sizeof(w));
    in.read(reinterpret_cast<char*>(&h), sizeof(h));

    bool constantHeat;
    in.read(reinterpret_cast<char*>(&constantHeat), sizeof(constantHeat));

    auto state = std::make_unique<SimulationState>();
    auto history = std::make_unique<SimulationHistory>();

    size_t history_count;
    in.read(reinterpret_cast<char*>(&history_count), sizeof(history_count));

    history->time_history.resize(history_count);
    history->max_temp_history.resize(history_count);
    history->min_temp_history.resize(history_count);

    in.read(reinterpret_cast<char*>(history->time_history.data()), history_count * sizeof(double));
    in.read(reinterpret_cast<char*>(history->max_temp_history.data()), history_count * sizeof(double));
    in.read(reinterpret_cast<char*>(history->min_temp_history.data()), history_count * sizeof(double));

    history->temperature_history.resize(history_count, std::vector<double>(state->cells));
    for (size_t i = 0; i < history_count; ++i) {
        in.read(reinterpret_cast<char*>(history->temperature_history[i].data()), state->cells * sizeof(double));
    }

    for (int d = 0; d < 9; ++d) {
        in.read(reinterpret_cast<char*>(state->grid.g.data() + d * state->cells), state->cells * sizeof(double));
    }

    if (history_count > 0) {
        state->current_step = history->time_history.back();
        state->temperatures = history->temperature_history.back();
    }

    in.close();
    return std::make_unique<LocalCUDAEngine>(w, h, constantHeat, std::move(state), std::move(history));
}

#endif
