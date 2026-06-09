#pragma once
// AoS (Array of Structures) kernel variants for RQ3 comparison.
// Layout: single array gf[2*9*cells], where gf[i*18 + d] = g[d][i], gf[i*18 + 9 + d] = f[d][i]
// Each cell's 18 doubles are contiguous in memory.
// On GPU this causes uncoalesced access, warp threads are 144 bytes apart.

#ifdef __CUDACC__

__global__ void collisionKernelAoS(int height, int width,
                                   double tauT, double TempAvg, double tauF,
                                   double lattice_buoyancy,
                                   double* gf_new, const double* gf_old);

__global__ void streamKernelAoS(int height, int width,
                                double* gf_new, const double* gf_old,
                                const bool* isRad);

#endif
