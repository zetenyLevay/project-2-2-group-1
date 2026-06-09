#include "kernels.cuh"
#include "main.h"

// D2Q9 lattice constants in constant memory
__constant__ double d_cx[9];
__constant__ double d_cy[9];
__constant__ double d_weights[9];
__constant__ double d_cs2;
__constant__ int    d_inv[9];
__constant__ double d_room_temp;
__constant__ double d_max_temp;


void initCudaLattice() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    const double h_cx[9]     = {0.0, 1.0, 0.0, -1.0, 0.0,  1.0, -1.0, -1.0,  1.0};
    const double h_cy[9]     = {0.0, 0.0, 1.0,  0.0, -1.0, 1.0,  1.0, -1.0, -1.0};
    const double h_weights[9]= {4.0/9.0,
                                1.0/9.0, 1.0/9.0, 1.0/9.0, 1.0/9.0,
                                1.0/36.0, 1.0/36.0, 1.0/36.0, 1.0/36.0};
    const int    h_inv[9]     = {0, 3, 4, 1, 2, 7, 8, 5, 6};
    double h_cs2 = 1.0 / 3.0;

    cudaMemcpyToSymbol(d_cx,      h_cx,      9 * sizeof(double));
    cudaMemcpyToSymbol(d_cy,      h_cy,      9 * sizeof(double));
    cudaMemcpyToSymbol(d_weights, h_weights, 9 * sizeof(double));
    cudaMemcpyToSymbol(d_inv,     h_inv,     9 * sizeof(int));
    cudaMemcpyToSymbol(d_cs2,     &h_cs2,    sizeof(double));
    cudaMemcpyToSymbol(d_room_temp, &ROOM_TEMP, sizeof(double));
    double h_max_temp = MAX_TEMP;
    cudaMemcpyToSymbol(d_max_temp, &h_max_temp, sizeof(double));
}


__device__ void getDensityAndVelocity(const double* f_old, int idx, int n_cells,
                                      double& density, double& ux, double& uy) {
    density = 0.0; ux = 0.0; uy = 0.0;
    for (int d = 0; d < 9; ++d) {
        double val = f_old[d * n_cells + idx];
        density += val;
        ux += val * d_cx[d];
        uy += val * d_cy[d];
    }
    if (density != 0.0) { ux /= density; uy /= density; }
}

__device__ double atomicMaxD(double* addr, double val) {
    double old = *addr;
    while (val > old) {
        double assumed = old;
        old = __longlong_as_double(atomicCAS(
            (unsigned long long*)addr,
            __double_as_longlong(assumed),
            __double_as_longlong(val)));
    }
    return old;
}

__device__ double atomicMinD(double* addr, double val) {
    double old = *addr;
    while (val < old) {
        double assumed = old;
        old = __longlong_as_double(atomicCAS(
            (unsigned long long*)addr,
            __double_as_longlong(assumed),
            __double_as_longlong(val)));
    }
    return old;
}

__device__ double atomicAddD(double* addr, double val) {
    double old = *addr;
    double assumed;
    do {
        assumed = old;
        old = __longlong_as_double(atomicCAS(
            (unsigned long long*)addr,
            __double_as_longlong(assumed),
            __double_as_longlong(assumed + val)));
    } while (__double_as_longlong(old) != __double_as_longlong(assumed));
    return old;
}


__global__ void collisionKernel(int height, int width,
                                double tauT, double TempAvg, double tauF,
                                double lattice_buoyancy,
                                double* g_new, double* f_new,
                                const double* g_old, const double* f_old) {

    int x = blockDim.x * blockIdx.x + threadIdx.x;
    int y = blockDim.y * blockIdx.y + threadIdx.y;
    if (x >= width || y >= height) return;

    int n_cells = width * height;
    int idx = y * width + x;

    double density, ux, uy;
    getDensityAndVelocity(f_old, idx, n_cells, density, ux, uy);

    double temp = 0.0;
    for (int d = 0; d < 9; ++d) temp += g_old[d * n_cells + idx];

    double buoyancy = lattice_buoyancy * (temp - d_room_temp);
    double uyF = 0.0;
    if (density != 0.0) uyF = uy + 0.5 * buoyancy / density;

    for (int d = 0; d < 9; ++d) {
        int mem = d * n_cells + idx;

        double cuF = d_cx[d] * ux + d_cy[d] * uyF;
        double forceTerm = d_weights[d] * (1.0 - 0.5 / tauF) *
            (((d_cy[d] - uyF) * buoyancy) / d_cs2 +
             ((d_cx[d] * ux + d_cy[d] * uyF) * (d_cy[d] * buoyancy)) / (d_cs2 * d_cs2));

        double feq = d_weights[d] * density *
            (1.0 + cuF / d_cs2 + (cuF * cuF) / (2.0 * d_cs2 * d_cs2) -
             (ux * ux + uyF * uyF) / (2.0 * d_cs2));

        f_new[mem] = f_old[mem] - (1.0 / tauF) * (f_old[mem] - feq) + forceTerm;

        double cuT = d_cx[d] * ux + d_cy[d] * uy;
        double geq = d_weights[d] * temp * (1.0 + cuT / d_cs2);
        g_new[mem] = g_old[mem] - (1.0 / tauT) * (g_old[mem] - geq);
    }
}

__global__ void streamKernel(int height, int width,
                             double* g_new, double* f_new,
                             const double* g_old, const double* f_old,
                             const bool* isRad) {

    int x = blockDim.x * blockIdx.x + threadIdx.x;
    int y = blockDim.y * blockIdx.y + threadIdx.y;
    if (x >= width || y >= height) return;

    int n_cells = width * height;
    int idx = y * width + x;

    if (isRad[idx]) return;

    for (int d = 0; d < 9; ++d) {
        int srcX = x - (int)d_cx[d];
        int srcY = y - (int)d_cy[d];
        int mem = d * n_cells + idx;

        if (srcX >= 0 && srcY >= 0 && srcX < width && srcY < height) {
            int srcIdx = srcY * width + srcX;

            if (isRad[srcIdx]) {
                int opp = d_inv[d];
                int oppMem = opp * n_cells + srcIdx;
                double radTemp = 0.0;
                for (int k = 0; k < 9; ++k)
                    radTemp += g_old[k * n_cells + srcIdx];
                g_new[mem] = -g_old[oppMem] + 2.0 * d_weights[d] * radTemp;
                f_new[mem] = f_old[oppMem];
            } else {
                int srcMem = d * n_cells + srcIdx;
                g_new[mem] = g_old[srcMem];
                f_new[mem] = f_old[srcMem];
            }
        } else {
            int opp = d_inv[d];
            int oppMem = opp * n_cells + idx;
            g_new[mem] = g_old[oppMem];
            f_new[mem] = f_old[oppMem];
        }
    }
}


__global__ void initScalarsKernel(double* max_temp, double* min_temp,
                                  double* temp_sum) {
    *max_temp = -1e100;
    *min_temp =  1e100;
    *temp_sum = 0.0;
}

__global__ void reduceTemperatureKernel(int cells,
                                        const double* g,
                                        double* temperatures,
                                        double* max_temp,
                                        double* min_temp,
                                        double* temp_sum) {

    __shared__ double block_max[256];
    __shared__ double block_min[256];
    __shared__ double block_sum[256];

    int tid = threadIdx.x;
    int idx = blockIdx.x * blockDim.x + tid;

    double local_max = -1e100;
    double local_min =  1e100;
    double local_sum = 0.0;

    for (int i = idx; i < cells; i += gridDim.x * blockDim.x) {
        double temp = 0.0;
        for (int d = 0; d < 9; ++d) temp += g[d * cells + i];
        if (temperatures) temperatures[i] = temp;
        if (temp > local_max) local_max = temp;
        if (temp < local_min) local_min = temp;
        local_sum += temp;
    }

    block_max[tid] = local_max;
    block_min[tid] = local_min;
    block_sum[tid] = local_sum;
    __syncthreads();

    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            if (block_max[tid + s] > block_max[tid]) block_max[tid] = block_max[tid + s];
            if (block_min[tid + s] < block_min[tid]) block_min[tid] = block_min[tid + s];
            block_sum[tid] += block_sum[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        atomicMaxD(max_temp, block_max[0]);
        atomicMinD(min_temp, block_min[0]);
        atomicAddD(temp_sum, block_sum[0]);
    }
}


__global__ void heatSourcePatchKernel(const int* heat_sources,
                                      int n_heat_sources,
                                      double max_temp,
                                      const double* temperatures,
                                      int stride,
                                      double* g, double* f) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n_heat_sources) return;

    int idx = heat_sources[i];
    double currentTemp = 0.0;
    for (int d = 0; d < 9; ++d) currentTemp += g[d * stride + idx];

    if (currentTemp < max_temp) {
        currentTemp += 0.0005;
        if (currentTemp > max_temp) currentTemp = max_temp;
    }

    for (int d = 0; d < 9; ++d) {
        g[d * stride + idx] = d_weights[d] * currentTemp;
        f[d * stride + idx] = d_weights[d] * 1.0;
    }
}


__global__ void applyRadiationFluxKernel(int n_cells,
                                         const int* boundary_indices,
                                         const double* flux,
                                         int n_boundary,
                                         double* g) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n_boundary) return;

    int idx = boundary_indices[i];
    double f = flux[i];
    for (int d = 0; d < 9; ++d) {
        g[d * n_cells + idx] += d_weights[d] * f;
    }
}

__global__ void computeT4Kernel(int cells,
                                const double* temperatures,
                                double* t4) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= cells) return;
    double tk = temperatures[i] + 273.15;
    double tk2 = tk * tk;
    t4[i] = tk2 * tk2;
}

__global__ void computeRadiationFluxKernel(int n_viewFactors,
                                           const int* vf_source,
                                           const int* vf_target,
                                           const double* vf_factor,
                                           const double* t4,
                                           const int* cellToBoundary,
                                           double lattice_stefan_boltzmann,
                                           double* boundary_flux,
                                           double* total_radiation) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n_viewFactors) return;

    double heatFlux = lattice_stefan_boltzmann * vf_factor[i] *
        (t4[vf_source[i]] - t4[vf_target[i]]);
    heatFlux /= 50.0;

    int bi = cellToBoundary[vf_target[i]];
    if (bi >= 0) atomicAddD(&boundary_flux[bi], heatFlux);
    atomicAddD(total_radiation, heatFlux);
}

// CSR radiation kernel: one thread per boundary cell, zero atomics on flux.
// View factors are pre-grouped by target — each thread reads its segment sequentially.
__global__ void radiationFluxKernelCSR(int n_boundary,
                                       const int* boundary_indices,
                                       const int* vf_start,
                                       const int* vf_source,
                                       const double* vf_factor,
                                       const double* t4,
                                       double lattice_stefan_boltzmann,
                                       double* boundary_flux,
                                       double* total_radiation) {
    int bi = blockIdx.x * blockDim.x + threadIdx.x;
    if (bi >= n_boundary) return;

    int start = vf_start[bi];
    int end   = vf_start[bi + 1];
    int target_cell = boundary_indices[bi];
    double t4_target = t4[target_cell];

    double total = 0.0;
    for (int j = start; j < end; ++j) {
        double hf = lattice_stefan_boltzmann * vf_factor[j] *
            (t4[vf_source[j]] - t4_target);
        hf /= 50.0;
        total += hf;
    }

    boundary_flux[bi] = total;
    atomicAddD(total_radiation, total);
}
