#include "CudaEngine.h"
#include "kernels.cuh"
#include <cuda_runtime.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <chrono>
#include <vector>
#include <cmath>

CudaEngine::CudaEngine(int width, int height)
    : width(width), height(height), cells(width * height), n_vals(9 * cells) {

    size_t bytes = n_vals * sizeof(double);
    cudaMalloc(&g_src, bytes);
    cudaMalloc(&f_src, bytes);
    cudaMalloc(&g_mid, bytes);
    cudaMalloc(&f_mid, bytes);
    cudaMalloc(&g_dst, bytes);
    cudaMalloc(&f_dst, bytes);
}

CudaEngine::~CudaEngine() {
    cudaFree(g_src); cudaFree(f_src);
    cudaFree(g_mid); cudaFree(f_mid);
    cudaFree(g_dst); cudaFree(f_dst);
}

void CudaEngine::pack(const Grid& grid) {
    for (int d = 0; d < 9; ++d) {
        cudaMemcpy(g_src + d * cells, grid.g[d].data(),
                   cells * sizeof(double), cudaMemcpyHostToDevice);
        cudaMemcpy(f_src + d * cells, grid.f[d].data(),
                   cells * sizeof(double), cudaMemcpyHostToDevice);
    }
}

void CudaEngine::unpack(Grid& grid) const {
    for (int d = 0; d < 9; ++d) {
        cudaMemcpy(grid.g[d].data(), g_dst + d * cells,
                   cells * sizeof(double), cudaMemcpyDeviceToHost);
        cudaMemcpy(grid.f[d].data(), f_dst + d * cells,
                   cells * sizeof(double), cudaMemcpyDeviceToHost);
    }
}

void CudaEngine::collision(double tauT, double TempAvg, double tauF) {
    runCudaCollision(height, width, tauT, TempAvg, tauF,
                     g_mid, f_mid, g_src, f_src);
}

void CudaEngine::stream() {
    runCudaStream(height, width, g_dst, f_dst, g_mid, f_mid);
}

void CudaEngine::step(Grid& grid, double tauT, double TempAvg, double tauF) {
    pack(grid);
    collision(tauT, TempAvg, tauF);
    stream();
    unpack(grid);
}

// CPU reference functions, used for correctness & timing
static void cpuCollision(int height, int width,
                         double tauT, double TempAvg, double tauF,
                         double* g_new, double* f_new,
                         const double* g_old, const double* f_old) {
    const int    cx[9]     = {0, 1, 0, -1, 0,  1, -1, -1,  1};
    const int    cy[9]     = {0, 0, 1,  0, -1, 1,  1, -1, -1};
    const double weights[9]= {4.0/9.0,
                              1.0/9.0, 1.0/9.0, 1.0/9.0, 1.0/9.0,
                              1.0/36.0, 1.0/36.0, 1.0/36.0, 1.0/36.0};
    const double cs2 = 1.0 / 3.0;

    double inv_tauF = 1.0 / tauF;
    double inv_tauT = 1.0 / tauT;
    double half_inv_tauF = 1.0 - 0.5 / tauF;
    int n_cells = width * height;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;

            double density = 0.0, ux = 0.0, uy = 0.0;
            for (int d = 0; d < 9; ++d) {
                double val = f_old[d * n_cells + idx];
                density += val;
                ux += val * cx[d];
                uy += val * cy[d];
            }
            if (density != 0.0) { ux /= density; uy /= density; }

            double temp = 0.0;
            for (int d = 0; d < 9; ++d)
                temp += g_old[d * n_cells + idx];

            double buoyancy = 4.0e-5 * (temp - TempAvg);
            double uyF = (density != 0.0) ? uy + 0.5 * buoyancy : 0.0;

            for (int d = 0; d < 9; ++d) {
                int mem = d * n_cells + idx;
                double cuF  = cx[d] * ux  + cy[d] * uyF;
                double cuT  = cx[d] * ux  + cy[d] * uy;
                double cuF2 = cuF * cuF;
                double u2F  = ux * ux + uyF * uyF;

                double feq = weights[d] * density *
                    (1.0 + cuF / cs2 + cuF2 / (2.0 * cs2 * cs2) - u2F / (2.0 * cs2));

                double forceTerm = weights[d] * half_inv_tauF *
                    (((cy[d] - uy) * buoyancy) / cs2 +
                     (cuF * (cy[d] * buoyancy)) / (cs2 * cs2));

                f_new[mem] = f_old[mem] - inv_tauF * (f_old[mem] - feq) + forceTerm;

                double geq = weights[d] * temp * (1.0 + cuT / cs2);
                g_new[mem] = g_old[mem] - inv_tauT * (g_old[mem] - geq);
            }
        }
    }
}

static void cpuStream(int height, int width,
                      double* g_new, double* f_new,
                      const double* g_old, const double* f_old) {
    const int cx[9] = {0, 1, 0, -1, 0, 1, -1, -1, 1};
    const int cy[9] = {0, 0, 1, 0, -1, 1, 1, -1, -1};
    const int inv[9] = {0, 3, 4, 1, 2, 7, 8, 5, 6};
    int n_cells = width * height;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            for (int d = 0; d < 9; ++d) {
                int srcX = x - cx[d];
                int srcY = y - cy[d];
                int mem = d * n_cells + idx;

                if (srcX >= 0 && srcY >= 0 && srcX < width && srcY < height) {
                    int srcIdx = srcY * width + srcX;
                    g_new[mem] = g_old[d * n_cells + srcIdx];
                    f_new[mem] = f_old[d * n_cells + srcIdx];
                } else {
                    int opp = inv[d];
                    g_new[mem] = g_old[opp * n_cells + idx];
                    f_new[mem] = f_old[opp * n_cells + idx];
                }
            }
        }
    }
}

// Helpers
static void gridToFlat(const Grid& grid, double* g, double* f, int cells) {
    for (int d = 0; d < 9; ++d) {
        std::memcpy(g + d * cells, grid.g[d].data(), cells * sizeof(double));
        std::memcpy(f + d * cells, grid.f[d].data(), cells * sizeof(double));
    }
}

static double maxError(int n, const double* a, const double* b) {
    double err = 0.0;
    for (int i = 0; i < n; ++i) {
        double e = fabs(a[i] - b[i]);
        if (isnan(e)) return NAN;
        if (e > err) err = e;
    }
    return err;
}

// Benchmark
int main() {
    initCudaLattice();

    const double tauT = 0.8, tauF = 0.6, TempAvg = 20.0;
    const int bench_steps = 500;

    struct Config { int w, h; };
    const Config configs[] = {
        {64, 64},
        {128, 128},
        {200, 200},
        {400, 400},
    };

    printf("%-12s %8s %12s %12s %12s %8s\n",
           "Grid", "Steps", "CPU (ms)", "GPU (ms)", "Speedup", "MaxErr");
    printf("%s\n", std::string(68, '-').c_str());

    for (auto [w, h] : configs) {
        int cells = w * h;
        int n_vals = 9 * cells;

        // Allocate flat buffers
        std::vector<double> g_cpu(cells * 9), f_cpu(cells * 9);
        std::vector<double> g_tmp(cells * 9), f_tmp(cells * 9);

        // Start with uniform equilibrium
        Grid gridInit(cells);
        for (int i = 0; i < cells; ++i) {
            for (int d = 0; d < 9; ++d) {
                gridInit.g[d][i] = weights[d] * 20.0;
                gridInit.f[d][i] = weights[d] * 1.0;
            }
        }

        // CPU benchmark
        gridToFlat(gridInit, g_cpu.data(), f_cpu.data(), cells);

        auto t0 = std::chrono::high_resolution_clock::now();
        for (int s = 0; s < bench_steps; ++s) {
            cpuCollision(h, w, tauT, TempAvg, tauF,
                         g_tmp.data(), f_tmp.data(),
                         g_cpu.data(), f_cpu.data());
            cpuStream(h, w,
                      g_cpu.data(), f_cpu.data(),
                      g_tmp.data(), f_tmp.data());
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double cpu_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        // GPU benchmark (warmup first)
        CudaEngine engine(w, h);
        Grid gridGpu(cells);

        // Warmup: 5 steps on dummy data
        {
            Grid warmGrid(cells);
            for (int i = 0; i < cells; ++i)
                for (int d = 0; d < 9; ++d)
                    warmGrid.g[d][i] = warmGrid.f[d][i] = 1.0;
            for (int s = 0; s < 5; ++s)
                engine.step(warmGrid, tauT, TempAvg, tauF);
        }

        // Reset to initial state so CPU and GPU start from the same data
        for (int i = 0; i < cells; ++i) {
            for (int d = 0; d < 9; ++d) {
                gridGpu.g[d][i] = gridInit.g[d][i];
                gridGpu.f[d][i] = gridInit.f[d][i];
            }
        }

        auto t2 = std::chrono::high_resolution_clock::now();
        for (int s = 0; s < bench_steps; ++s)
            engine.step(gridGpu, tauT, TempAvg, tauF);
        auto t3 = std::chrono::high_resolution_clock::now();
        double gpu_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();

        //  Correctness: Compare GPU result to CPU result
        std::vector<double> g_gpu_flat(cells * 9), f_gpu_flat(cells * 9);
        gridToFlat(gridGpu, g_gpu_flat.data(), f_gpu_flat.data(), cells);

        double err_g = maxError(n_vals, g_cpu.data(), g_gpu_flat.data());
        double err_f = maxError(n_vals, f_cpu.data(), f_gpu_flat.data());
        double max_err = std::max(err_g, err_f);

        double speedup = cpu_ms / gpu_ms;

        char label[16];
        snprintf(label, sizeof(label), "%dx%d", w, h);
        printf("%-12s %8d %12.2f %12.2f %11.2fx %8.2e\n",
               label, bench_steps, cpu_ms, gpu_ms, speedup, max_err);
    }
    return 0;
}
