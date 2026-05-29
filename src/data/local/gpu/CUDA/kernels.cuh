#pragma once

// One-time upload of D2Q9 lattice constants to device __constant__ memory.
// Must be called once before any kernel run.
void initCudaLattice();

// Host launchers for the kernels.
void runCudaCollision(int height, int width,
                      double tauT, double TempAvg, double tauF,
                      double* g_new, double* f_new,
                      const double* g_old, const double* f_old);

void runCudaStream(int height, int width,
                   double* g_new, double* f_new,
                   const double* g_old, const double* f_old);
