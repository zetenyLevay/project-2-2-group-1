// AoS engine
// Minimal implementation for RQ3 benchmark comparison.
// Uses interleaved gf layout: gf[cell*18 + d] = g_d, gf[cell*18 + 9 + d] = f_d

#if CUDA_AVAILABLE == 1

#include "./LocalCUDAEngineAoS.cuh"
#include "LocalEngine.h"
#include <numeric>
#include <cstring>
#include <iostream>

#define STRIDE 18

static void aoS_to_soa_grid(const std::vector<double>& g, const std::vector<double>& f,
                             std::vector<double>& gf, int cells) {
    gf.resize(cells * STRIDE);
    for (int i = 0; i < cells; ++i) {
        for (int d = 0; d < 9; ++d) {
            gf[i * STRIDE + d] = g[d * cells + i];
            gf[i * STRIDE + 9 + d] = f[d * cells + i];
        }
    }
}

void LocalCUDAEngineAoS::initializeCuda() {
    initCudaLattice();
    size_t bytes = (size_t)cells * STRIDE * sizeof(double);
    cudaMalloc(&d_gf_src, bytes);
    cudaMalloc(&d_gf_mid, bytes);
    cudaMalloc(&d_gf_dst, bytes);

    cudaMalloc(&d_temperatures, cells * sizeof(double));
    cudaMalloc(&d_max_temp, sizeof(double));
    cudaMalloc(&d_min_temp, sizeof(double));
    cudaMalloc(&d_temp_sum, sizeof(double));
    cudaMalloc(&d_isRad, cells * sizeof(bool));

    d_boundary_indices = nullptr;
    d_rad_flux = nullptr; d_t4 = nullptr; d_cellToBoundary = nullptr;
    d_vf_source = nullptr; d_vf_target = nullptr; d_vf_factor = nullptr;
    d_total_radiation = nullptr;
    n_boundary = 0; n_viewFactors = 0;

    cudaMallocHost(&h_pinned_temps, cells * sizeof(double));
}

void LocalCUDAEngineAoS::uploadInitialGridAoS() {
    auto state = getState();
    std::vector<double> gf;
    aoS_to_soa_grid(state->grid.g, state->grid.f, gf, cells);
    cudaMemcpy(d_gf_src, gf.data(), cells * STRIDE * sizeof(double), cudaMemcpyHostToDevice);
}

LocalCUDAEngineAoS::LocalCUDAEngineAoS(int w, int h, bool constantHeatSource)
    : SimulationEngine(w, h), stride(STRIDE) {
    initializeCuda();

    auto initialState = std::make_unique<SimulationState>();
    initialState->width = w; initialState->height = h;
    auto cells = initialState->cells = w * h;
    initialState->grid = Grid(cells);
    initialState->current_step = 0;
    initialState->isConstantHeatSource = constantHeatSource;

    initPhysics(*initialState, w, h);
    initialState->temperatures.resize(cells, ROOM_TEMP);
    for (int idx : initialState->heatSources)
        initialState->temperatures[idx] = ROOM_TEMP;

    double tAvg = 0;
    for (int i = 0; i < cells; ++i) {
        for (int d = 0; d < 9; ++d) {
            initialState->grid.g[d * cells + i] = weights[d] * initialState->temperatures[i];
            initialState->grid.f[d * cells + i] = weights[d] * 1.0;
        }
        tAvg += initialState->temperatures[i];
    }
    initialState->TempAvg = tAvg / cells;

    history = std::make_unique<SimulationHistory>();
    history->time_history.push_back(0);
    history->max_temp_history.push_back(ROOM_TEMP);
    history->min_temp_history.push_back(ROOM_TEMP);
    history->temperature_history.push_back(initialState->temperatures);
    history->convectionOutput.push_back(0.0);
    history->radiationOutput.push_back(0.0);

    // GPU radiation buffers (reuse SoA engine pattern)
    n_boundary = initialState->boundaryCells.size();
    n_viewFactors = initialState->viewFactors.size();
    cudaMalloc(&d_t4, cells * sizeof(double));
    cudaMalloc(&d_cellToBoundary, cells * sizeof(int));
    cudaMalloc(&d_total_radiation, sizeof(double));
    cudaMemcpy(d_cellToBoundary, initialState->cellToBoundary.data(),
               cells * sizeof(int), cudaMemcpyHostToDevice);
    if (n_boundary > 0) {
        cudaMalloc(&d_boundary_indices, n_boundary * sizeof(int));
        cudaMalloc(&d_rad_flux, n_boundary * sizeof(double));
        cudaMemcpy(d_boundary_indices, initialState->boundaryCells.data(),
                   n_boundary * sizeof(int), cudaMemcpyHostToDevice);
    }
    if (n_viewFactors > 0) {
        std::vector<int> src(n_viewFactors), tgt(n_viewFactors);
        std::vector<double> fac(n_viewFactors);
        for (int i = 0; i < n_viewFactors; ++i) {
            src[i] = initialState->viewFactors[i].sourceIdx;
            tgt[i] = initialState->viewFactors[i].targetIdx;
            fac[i] = initialState->viewFactors[i].factor;
        }
        cudaMalloc(&d_vf_source, n_viewFactors * sizeof(int));
        cudaMalloc(&d_vf_target, n_viewFactors * sizeof(int));
        cudaMalloc(&d_vf_factor, n_viewFactors * sizeof(double));
        cudaMemcpy(d_vf_source, src.data(), n_viewFactors * sizeof(int), cudaMemcpyHostToDevice);
        cudaMemcpy(d_vf_target, tgt.data(), n_viewFactors * sizeof(int), cudaMemcpyHostToDevice);
        cudaMemcpy(d_vf_factor, fac.data(), n_viewFactors * sizeof(double), cudaMemcpyHostToDevice);
    }

    thread = std::make_unique<ReusableThread>(std::move(initialState));
    uploadInitialGridAoS();
    cudaMemcpy(d_isRad, getState()->isRad.data(), cells * sizeof(bool), cudaMemcpyHostToDevice);
    launchReduce();
}

LocalCUDAEngineAoS::~LocalCUDAEngineAoS() {
    cudaFree(d_gf_src); cudaFree(d_gf_mid); cudaFree(d_gf_dst);
    cudaFree(d_temperatures); cudaFree(d_max_temp); cudaFree(d_min_temp); cudaFree(d_temp_sum);
    cudaFree(d_isRad);
    if (d_boundary_indices) cudaFree(d_boundary_indices);
    if (d_rad_flux) cudaFree(d_rad_flux);
    if (d_t4) cudaFree(d_t4);
    if (d_cellToBoundary) cudaFree(d_cellToBoundary);
    if (d_vf_source) cudaFree(d_vf_source);
    if (d_vf_target) cudaFree(d_vf_target);
    if (d_vf_factor) cudaFree(d_vf_factor);
    if (d_total_radiation) cudaFree(d_total_radiation);
    cudaFreeHost(h_pinned_temps);
}

void LocalCUDAEngineAoS::collision(double tauT, double TempAvg, double tauF, double lb) {
    dim3 block(16, 16);
    dim3 grid((width + 15) / 16, (height + 15) / 16);
    collisionKernelAoS<<<grid, block>>>(height, width, tauT, TempAvg, tauF, lb,
                                        d_gf_mid, d_gf_src);
    cudaDeviceSynchronize();
}

void LocalCUDAEngineAoS::stream() {
    dim3 block(16, 16);
    dim3 grid((width + 15) / 16, (height + 15) / 16);
    streamKernelAoS<<<grid, block>>>(height, width, d_gf_dst, d_gf_mid, d_isRad);
    cudaDeviceSynchronize();
}

void LocalCUDAEngineAoS::launchReduce() {
    initScalarsKernel<<<1, 1>>>(d_max_temp, d_min_temp, d_temp_sum);
    // For reduce we use the SoA style g array from d_gf_src then extract g portion
    // The reduce kernel expects g[d*cells + i]. We calculate temperatures from g in d_gf_src.
    // Since d_gf_src has g and f interleaved, we can't directly use reduceTemperatureKernel.
    // Instead compute temperatures on CPU from d_temperatures.
    // For simplicity in benchmark: skip GPU reduce, use a simple kernel.
    (void)d_temperatures; // reduce is not critical for AoS MLUPS measurement
    cudaDeviceSynchronize();
}

void LocalCUDAEngineAoS::launchHeatSourcePatch(const SimulationState& state) {
    // Simplified — skip for benchmark throughput measurement
    (void)state;
}

void LocalCUDAEngineAoS::launchRadiationFlux(const SimulationState& state) {
    // Simplified — skip for benchmark throughput measurement
    (void)state;
}

void LocalCUDAEngineAoS::stepFoward() {
    thread->submitTask([this](const SimulationState& prev, SimulationState& next) {
        if (prev.current_step < (int)history->temperature_history.size() - 1) {
            next.current_step = prev.current_step + 1;
            next.temperatures = history->temperature_history[next.current_step];
            return;
        }

        collision(prev.tauT, prev.TempAvg, prev.tauF, prev.lattice_buoyancy);
        stream();
        swapDeviceGrids();

        next.TempAvg = prev.TempAvg;
        next.current_step = prev.current_step + 1;

        if (getAutoPlayStatus()) stepFoward();
    });
}

void LocalCUDAEngineAoS::stepBack() {
    thread->submitTask([this](const SimulationState& prev, SimulationState& next) {
        if (prev.current_step <= 0) return;
        next.current_step = prev.current_step - 1;
        next.temperatures = history->temperature_history[next.current_step];
    });
}

void LocalCUDAEngineAoS::seekTo(int step) {
    thread->submitTask([this, step](const SimulationState& prev, SimulationState& next) {
        if (step < 0 || step >= (int)history->temperature_history.size()) return;
        next.current_step = step;
        next.temperatures = history->temperature_history[next.current_step];
    });
}

double LocalCUDAEngineAoS::getTotalEnergy() const {
    auto state = thread->getState();
    return std::accumulate(state->temperatures.begin(), state->temperatures.end(), 0.0);
}

#endif
