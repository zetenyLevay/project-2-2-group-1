#pragma once
// AoS (Array of Structures) engine variant for RQ3 memory layout comparison.
// Uses a single interleaved gf array: gf[cell*18 + d] = g_d_cell, gf[cell*18 + 9 + d] = f_d_cell
// This is the UNOPTIMIZED layout, included only for benchmarking AoS vs SoA.

#include "LocalCUDAEngine.cuh"
#include "kernels_aos.cuh"

class LocalCUDAEngineAoS : public SimulationEngine {
public:
    LocalCUDAEngineAoS(int w, int h, bool constantHeatSource);
    ~LocalCUDAEngineAoS();

    void stepFoward();
    void stepBack();
    void seekTo(int step);
    double getTotalEnergy() const;

    bool batchMode = false;

private:
    const int stride;  // 18 per cell

    // AoS layout: single gf array per buffer (interleaved g + f per cell)
    double *d_gf_src, *d_gf_mid, *d_gf_dst;

    // Reduction + boundary (reuse same structure as SoA engine where possible)
    double *d_temperatures;
    double *d_max_temp, *d_min_temp, *d_temp_sum;
    bool   *d_isRad;
    int    *d_boundary_indices;
    double *d_rad_flux;
    int     n_boundary;
    double *d_t4;
    int    *d_cellToBoundary;
    int    *d_vf_source, *d_vf_target;
    double *d_vf_factor;
    int     n_viewFactors;
    double *d_total_radiation;
    double *h_pinned_temps;

    void initializeCuda();
    void uploadInitialGridAoS();
    void collision(double tauT, double TempAvg, double tauF, double lattice_buoyancy);
    void stream();
    void launchReduce();
    void launchHeatSourcePatch(const SimulationState& state);
    void launchRadiationFlux(const SimulationState& state);

    inline void swapDeviceGrids() {
        std::swap(d_gf_src, d_gf_dst);
    }
};
