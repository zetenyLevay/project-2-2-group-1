#pragma once

#include "kernels.cuh"
#include "ReusableThread.h"
#include "SimulationEngine.h"

class LocalCUDAEngine : public SimulationEngine {
    public:
        LocalCUDAEngine(int w, int h, bool constantHeatSource);
        LocalCUDAEngine(int w, int h, bool constantHeatSource, std::unique_ptr<SimulationState> initialState, std::unique_ptr<SimulationHistory> initialHistory);

        ~LocalCUDAEngine();

        void stepFoward();
        void stepBack();
        void seekTo(int step);
        double getTotalEnergy() const;

        bool batchMode = false;

    private:
        const int n_vals;

        // GPU LBM buffers (persistent across steps)
        double *g_src, *f_src;   // current grid -> collision input
        double *g_mid, *f_mid;   // collision output -> stream input
        double *g_dst, *f_dst;   // stream output -> becomes next g_src/f_src

        // GPU temperature reduction buffers
        double *d_temperatures;  // cells x double - per-cell temperature
        double *d_max_temp;      // scalar - max temperature this step
        double *d_min_temp;      // scalar - min temperature this step
        double *d_temp_sum;      // scalar - sum of all temperatures

        // GPU boundary / radiation data
        bool *d_isRad;              // cells x bool - radiator cell mask
        int  *d_boundary_indices;   // boundary cell indices for radiation
        double *d_rad_flux;         // radiation flux per boundary cell (accumulated per step)
        int    n_boundary;          // number of boundary cells
        double *d_t4;               // per-cell T^4 (calculate each step from temperatures)
        int   *d_cellToBoundary;    // cell index -> boundary index map (-1 = not boundary)
        // CSR view factor layout (grouped by target boundary cell)
        int   *d_vfStart;           // CSR start offsets (size n_boundary+1)
        int   *d_vfSourceCSR;       // CSR source cell indices
        double *d_vfFactorCSR;      // CSR view factor weights
        int    n_viewFactors;       // number of view factors
        double *d_total_radiation;  // scalar accumulator for total radiation this step

        // Host buffer for fast temperature download
        double *h_pinned_temps;

        // Upload initial grid once and download full grid only on save/rewind
        void uploadInitialGrid();
        void downloadFullGrid(Grid& grid) const;

        // Pointer swap, basically zero transfer, just swaps g_src<->g_dst, f_src<->f_dst
        inline void swapDeviceGrids() {
            std::swap(g_src, g_dst);
            std::swap(f_src, f_dst);
        }

        void collision(const double tauT, const double TempAvg, const double tauF, const double lattice_buoyancy);
        void stream();
        void launchReduce();
        void launchHeatSourcePatch(const SimulationState& state);
        void launchRadiationFlux(const SimulationState& state);
        void initializeCuda();
};

std::unique_ptr<LocalCUDAEngine> loadLocalCUDASimulation(const std::string& filepath);
