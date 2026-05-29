#pragma once

#include "main.h"
#include <cstddef>

class CudaEngine {
public:
    CudaEngine(int width, int height);
    ~CudaEngine();

    CudaEngine(const CudaEngine&) = delete;
    CudaEngine& operator=(const CudaEngine&) = delete;

    // Copy SoA data from a Grid into the internal source buffers.
    void pack(const Grid& grid);

    // Copy the internal destination buffers back into a Grid.
    void unpack(Grid& grid) const;

    // Run the BGK collision kernel: src -> mid.
    void collision(double tauT, double TempAvg, double tauF);

    // Run the bounce-back stream kernel: mid -> dst.
    void stream();

    // Full LBM step: pack -> collision -> stream -> unpack.
    void step(Grid& grid, double tauT, double TempAvg, double tauF);

    int getWidth()  const { return width; }
    int getHeight() const { return height; }
    int getCells()  const { return cells; }

private:
    int width, height, cells, n_vals;

    double *g_src, *f_src;   // input to collision
    double *g_mid, *f_mid;   // collision -> stream
    double *g_dst, *f_dst;   // stream -> unpack
};
