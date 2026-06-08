#if CUDA_AVAILABLE == 1

#include "./LocalCUDAEngine.cuh"
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
    this->d_vf_source = nullptr;
    this->d_vf_target = nullptr;
    this->d_vf_factor = nullptr;
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
    initialState->grid = initialState->cells;
    initialState->current_step = 0;
    initialState->heat_spread = tau_g;
    initialState->viscosity = lattice_kinematic_viscosity;
    initialState->tauF = initialState->viscosity * 3 + 0.5;
    initialState->tauT = initialState->heat_spread * 3 + 0.5;
    initialState->TempAvg = 0.0;
    initialState->heatSourceW = width * 0.1;
    initialState->heatSourceH = height * 0.4;
    initialState->isConstantHeatSource = constantHeatSource;
    initialState->temperatures.resize(cells, ROOM_TEMP);
    initialState->isRad.resize(cells, false);

    for (int y = radY; y < radY + initialState->heatSourceH && y < height; ++y) {
        for (int x = radX; x < radX + initialState->heatSourceW && x < width; ++x) {
            initialState->heatSources.push_back(getIndex(x, y));
        }
    }

    for (int idx : initialState->heatSources) {
        initialState->temperatures[idx] = ROOM_TEMP;
        initialState->isRad[idx] = true;
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

    // Radiator surface cells
    std::vector<int> surfaceR;
    for (int idx : initialState->heatSources) {
        int rX = idx % width;
        int rY = idx / width;
        bool isSurface = ((rX == radX) || (rX == radX + initialState->heatSourceW - 1) ||
                          (rY == radY) || (rY == radY + initialState->heatSourceH - 1));
        if (isSurface) surfaceR.push_back(idx);
    }

    // Boundary cells
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (x == 0 || x == width - 1 || y == 0 || y == height - 1) {
                if (!initialState->isRad[getIndex(x, y)]) {
                    initialState->boundaryCells.push_back(getIndex(x, y));
                }
            }
        }
    }

    initialState->cellToBoundary.resize(cells, -1);
    for (size_t i = 0; i < initialState->boundaryCells.size(); ++i) {
        initialState->cellToBoundary[initialState->boundaryCells[i]] = (int)i;
    }

    // View factors
    initialState->localFluxCache.resize(1, std::vector<double>(cells, 0.0));
    initialState->t4.resize(cells, 0.0);

    for (int radIdx : surfaceR) {
        int rX = radIdx % width;
        int rY = radIdx / width;
        double totalInverseDistance = 0.0;
        std::vector<double> temp(initialState->boundaryCells.size(), 0.0);

        for (size_t i = 0; i < initialState->boundaryCells.size(); i++) {
            int wallIdx = initialState->boundaryCells[i];
            int wallX = wallIdx % width;
            int wallY = wallIdx / width;
            double dist = std::sqrt(((rX - wallX) * (rX - wallX)) + ((rY - wallY) * (rY - wallY)));
            if (dist > 0) {
                temp[i] = 1.0 / dist;
                totalInverseDistance += temp[i];
            }
        }

        for (size_t i = 0; i < initialState->boundaryCells.size(); i++) {
            if (temp[i] > 0) {
                ViewFactor vf;
                vf.sourceIdx = radIdx;
                vf.targetIdx = initialState->boundaryCells[i];
                vf.factor = temp[i] / totalInverseDistance;
                initialState->viewFactors.push_back(vf);
            }
        }
    }

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

    if (this->n_viewFactors > 0) {
        std::vector<int> vf_source(this->n_viewFactors);
        std::vector<int> vf_target(this->n_viewFactors);
        std::vector<double> vf_factor(this->n_viewFactors);
        for (int i = 0; i < this->n_viewFactors; ++i) {
            vf_source[i] = initialState->viewFactors[i].sourceIdx;
            vf_target[i] = initialState->viewFactors[i].targetIdx;
            vf_factor[i] = initialState->viewFactors[i].factor;
        }
        cudaMalloc(&this->d_vf_source, this->n_viewFactors * sizeof(int));
        cudaMalloc(&this->d_vf_target, this->n_viewFactors * sizeof(int));
        cudaMalloc(&this->d_vf_factor, this->n_viewFactors * sizeof(double));
        cudaMemcpy(this->d_vf_source, vf_source.data(),
                   this->n_viewFactors * sizeof(int), cudaMemcpyHostToDevice);
        cudaMemcpy(this->d_vf_target, vf_target.data(),
                   this->n_viewFactors * sizeof(int), cudaMemcpyHostToDevice);
        cudaMemcpy(this->d_vf_factor, vf_factor.data(),
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

    if (this->n_viewFactors > 0) {
        std::vector<int> vf_source(this->n_viewFactors);
        std::vector<int> vf_target(this->n_viewFactors);
        std::vector<double> vf_factor(this->n_viewFactors);
        for (int i = 0; i < this->n_viewFactors; ++i) {
            vf_source[i] = state->viewFactors[i].sourceIdx;
            vf_target[i] = state->viewFactors[i].targetIdx;
            vf_factor[i] = state->viewFactors[i].factor;
        }
        cudaMalloc(&this->d_vf_source, this->n_viewFactors * sizeof(int));
        cudaMalloc(&this->d_vf_target, this->n_viewFactors * sizeof(int));
        cudaMalloc(&this->d_vf_factor, this->n_viewFactors * sizeof(double));
        cudaMemcpy(this->d_vf_source, vf_source.data(),
                   this->n_viewFactors * sizeof(int), cudaMemcpyHostToDevice);
        cudaMemcpy(this->d_vf_target, vf_target.data(),
                   this->n_viewFactors * sizeof(int), cudaMemcpyHostToDevice);
        cudaMemcpy(this->d_vf_factor, vf_factor.data(),
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
    if (this->d_vf_source) cudaFree(this->d_vf_source);
    if (this->d_vf_target) cudaFree(this->d_vf_target);
    if (this->d_vf_factor) cudaFree(this->d_vf_factor);
    if (this->d_total_radiation) cudaFree(this->d_total_radiation);
    cudaFreeHost(this->h_pinned_temps);
}



void LocalCUDAEngine::collision(const double tauT, const double TempAvg,
                                const double tauF) {
    dim3 block(16, 16);
    dim3 grid((this->width  + block.x - 1) / block.x,
              (this->height + block.y - 1) / block.y);

    collisionKernel<<<grid, block>>>(this->height, this->width,
                                     tauT, TempAvg, tauF,
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
    if (state.viewFactors.empty() || state.boundaryCells.empty()) return;

    // Compute T^4 and radiation flux on CPU. cellToBoundary gives O(1) lookup.
    std::vector<double> t4(state.cells);
    for (int i = 0; i < state.cells; ++i) {
        double tk = state.temperatures[i] + 273.15;
        double tk2 = tk * tk;
        t4[i] = tk2 * tk2;
    }

    std::vector<double> flux(state.boundaryCells.size(), 0.0);
    double radiationThisStep = 0.0;

    for (const auto& vf : state.viewFactors) {
        double heatFlux = lattice_stefan_boltzmann * vf.factor *
            (t4[vf.sourceIdx] - t4[vf.targetIdx]);
        heatFlux /= 50.0;
        radiationThisStep += heatFlux;

        int bi = state.cellToBoundary[vf.targetIdx];
        if (bi >= 0) flux[bi] += heatFlux;
    }

    history->radiationOutput.push_back(radiationThisStep);

    cudaMemcpy(this->d_rad_flux, flux.data(),
               this->n_boundary * sizeof(double), cudaMemcpyHostToDevice);

    int threads = std::min(256, this->n_boundary);
    int blocks = (this->n_boundary + threads - 1) / threads;
    applyRadiationFluxKernel<<<blocks, threads>>>(
        state.cells, this->d_boundary_indices, this->d_rad_flux,
        this->n_boundary, this->g_src);

    cudaDeviceSynchronize();
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
                        previousState.tauF);
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
