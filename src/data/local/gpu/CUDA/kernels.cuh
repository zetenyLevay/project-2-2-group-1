#pragma once

// One-time upload of D2Q9 lattice constants to device __constant__ memory.
void initCudaLattice();

#ifdef __CUDACC__

__global__ void collisionKernel(int height, int width,
                                double tauT, double TempAvg, double tauF,
                                double* g_new, double* f_new,
                                const double* g_old, const double* f_old);

__global__ void streamKernel(int height, int width,
                             double* g_new, double* f_new,
                             const double* g_old, const double* f_old,
                             const bool* isRad);

__global__ void reduceTemperatureKernel(int cells,
                                        const double* g,
                                        double* temperatures,
                                        double* max_temp,
                                        double* min_temp,
                                        double* temp_sum);

__global__ void heatSourcePatchKernel(const int* heat_sources,
                                      int n_heat_sources,
                                      double max_temp,
                                      const double* temperatures,
                                      int stride,
                                      double* g, double* f);

__global__ void initScalarsKernel(double* max_temp, double* min_temp,
                                  double* temp_sum);

__global__ void applyRadiationFluxKernel(int n_cells,
                                         const int* boundary_indices,
                                         const double* flux,
                                         int n_boundary,
                                         double* g);

__global__ void computeT4Kernel(int cells,
                                const double* temperatures,
                                double* t4);

__global__ void computeRadiationFluxKernel(int n_viewFactors,
                                           const int* vf_source,
                                           const int* vf_target,
                                           const double* vf_factor,
                                           const double* t4,
                                           const int* cellToBoundary,
                                           double* boundary_flux,
                                           double* total_radiation);

#endif
