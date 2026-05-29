#pragma once

#include "../../../SimulationEngine.h"

class LocalCUDAEngine : public SimulationEngine {
    public:
        LocalCUDAEngine(int w, int h, bool constantHeatSource);
        LocalCUDAEngine(int w, int h, bool constantHeatSource, std::unique_ptr<SimulationState> initialState, std::unique_ptr<SimulationHistory> initialHistory);

        ~LocalCUDAEngine();

        // Step foward one frame
        void stepFoward();

        void stepBack();

        void seekTo(int step);

        double getTotalEnergy() const;

    private:
        const int n_vals;

        double *g_src, *f_src;   // input to collision
        double *g_mid, *f_mid;   // collision -> stream
        double *g_dst, *f_dst;   // stream -> unpack

        void pack(const Grid& grid);
        void collision(const double tauT, const double TempAvg, const double tauF);
        void stream();
        void unpack(Grid& grid);

        void initCudaLattice();
        void initializeCuda();
};

__global__ void collisionKernel(int height, int width,
                                double tauT, double TempAvg, double tauF,
                                double* g_new, double* f_new,
                                const double* g_old, const double* f_old);

__global__ void streamKernel(int height, int width,
                             double* g_new, double* f_new,
                             const double* g_old, const double* f_old);