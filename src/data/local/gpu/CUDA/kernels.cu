#include <cuda_runtime.h>
#include <cstdio>
#include <cmath>
#include "kernels.cuh"

// D2Q9 lattice constants in constant memory
// Those are cached and broadcasted to all threads

__constant__ double d_cx[9];
__constant__ double d_cy[9];
__constant__ double d_weights[9];
__constant__ double d_cs2;
__constant__ int    d_inv[9];

// Device helpers
__device__ void getDensityAndVelocity(const double* f_old, int idx, int n_cells,
                                      double& density, double& ux, double& uy) {
    density = 0.0;
    ux = 0.0;
    uy = 0.0;
    for (int d = 0; d < 9; ++d) {
        double val = f_old[d * n_cells + idx];
        density += val;
        ux += val * d_cx[d];
        uy += val * d_cy[d];
    }
    if (density != 0.0) {
        ux /= density;
        uy /= density;
    }
}

__global__ void collisionKernel(int height, int width,
                                double tauT, double TempAvg, double tauF,
                                double* g_new, double* f_new,
                                const double* g_old, const double* f_old) {

    int x = blockDim.x * blockIdx.x + threadIdx.x;
    int y = blockDim.y * blockIdx.y + threadIdx.y;

    if (x >= width || y >= height) return;

    int n_cells = width * height;
    int idx = y * width + x;

    double inv_tauF = 1.0 / tauF;
    double inv_tauT = 1.0 / tauT;
    double inv_cs2 = 1.0 / d_cs2;
    double inv_cs4 = inv_cs2 * inv_cs2;
    double half_inv_tauF = 1.0 - 0.5 / tauF;

    double density, ux, uy;
    getDensityAndVelocity(f_old, idx, n_cells, density, ux, uy);

    double temp = 0.0;
    for (int d = 0; d < 9; ++d) {
        temp += g_old[d * n_cells + idx];
    }

    double buoyancy = 4.0e-5 * (temp - TempAvg);
    double uyF = (density != 0.0) ? uy + 0.5 * buoyancy : 0.0;

    for (int d = 0; d < 9; ++d) {
        int mem = d * n_cells + idx;

        double cuF  = d_cx[d] * ux  + d_cy[d] * uyF;
        double cuT  = d_cx[d] * ux  + d_cy[d] * uy;
        double cuF2 = cuF * cuF;
        double u2F  = ux * ux + uyF * uyF;

        double feq = d_weights[d] * density *
            (1.0 + cuF * inv_cs2 + cuF2 * 0.5 * inv_cs4 - u2F * 0.5 * inv_cs2);

        double forceTerm = d_weights[d] * half_inv_tauF *
            (((d_cy[d] - uy) * buoyancy) * inv_cs2 +
             (cuF * (d_cy[d] * buoyancy)) * inv_cs4);

        f_new[mem] = f_old[mem] - inv_tauF * (f_old[mem] - feq) + forceTerm;

        double geq = d_weights[d] * temp * (1.0 + cuT * inv_cs2);
        g_new[mem] = g_old[mem] - inv_tauT * (g_old[mem] - geq);
    }
}

// Host helper: upload lattice constants to __constant__ memory

void initCudaLattice() {
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
}

// Host helper: run the collision kernel
void runCudaCollision(int height, int width,
                      double tauT, double TempAvg, double tauF,
                      double* g_new, double* f_new,
                      const double* g_old, const double* f_old) {
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x,
              (height + block.y - 1) / block.y);

    collisionKernel<<<grid, block>>>(height, width,
                                     tauT, TempAvg, tauF,
                                     g_new, f_new,
                                     g_old, f_old);
    cudaError_t err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        fprintf(stderr, "collisionKernel error: %s\n", cudaGetErrorString(err));
        exit(1);
    }
}

__global__ void streamKernel(int height, int width,
                             double* g_new, double* f_new,
                             const double* g_old, const double* f_old) {

    int x = blockDim.x * blockIdx.x + threadIdx.x;
    int y = blockDim.y * blockIdx.y + threadIdx.y;

    if (x >= width || y >= height) return;

    int n_cells = width * height;
    int idx = y * width + x;

    for (int d = 0; d < 9; ++d) {
        int srcX = x - (int)d_cx[d];
        int srcY = y - (int)d_cy[d];
        int mem = d * n_cells + idx;

        if (srcX >= 0 && srcY >= 0 && srcX < width && srcY < height) {
            int srcIdx = srcY * width + srcX;
            int srcMem = d * n_cells + srcIdx;
            g_new[mem] = g_old[srcMem];
            f_new[mem] = f_old[srcMem];
        } else {
            int opp = d_inv[d];
            int oppMem = opp * n_cells + idx;
            g_new[mem] = g_old[oppMem];
            f_new[mem] = f_old[oppMem];
        }
    }
}

// Host helper: run the stream kernel
void runCudaStream(int height, int width,
                   double* g_new, double* f_new,
                   const double* g_old, const double* f_old) {
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x,
              (height + block.y - 1) / block.y);

    streamKernel<<<grid, block>>>(height, width,
                                  g_new, f_new,
                                  g_old, f_old);
    cudaError_t err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        fprintf(stderr, "streamKernel error: %s\n", cudaGetErrorString(err));
        exit(1);
    }
}
