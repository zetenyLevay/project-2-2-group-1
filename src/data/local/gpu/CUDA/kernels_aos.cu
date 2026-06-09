// AoS (Array of Structures) kernel variants for RQ3 memory layout comparison.
// Layout: gf[cell * 18 + d] = g distribution d at cell, gf[cell * 18 + 9 + d] = f
// Each cell's 18 doubles are contiguous therefore, uncoalesced GPU access.

#include "kernels_aos.cuh"
#include "kernels_shared.cuh"

#define STRIDE 18  // 9 g + 9 f per cell

__global__ void collisionKernelAoS(int height, int width,
                                   double tauT, double TempAvg, double tauF,
                                   double lattice_buoyancy,
                                   double* gf_new, const double* gf_old) {

    int x = blockDim.x * blockIdx.x + threadIdx.x;
    int y = blockDim.y * blockIdx.y + threadIdx.y;
    if (x >= width || y >= height) return;

    int n_cells = width * height;
    int idx = y * width + x;
    int base = idx * STRIDE;

    // density and velocity from f (AoS: gf_old[base + 9 + d])
    double density = 0.0, ux = 0.0, uy = 0.0;
    for (int d = 0; d < 9; ++d) {
        double val = gf_old[base + 9 + d];
        density += val;
        ux += val * d_cx[d];
        uy += val * d_cy[d];
    }
    if (density != 0.0) { ux /= density; uy /= density; }

    // temperature from g (AoS: gf_old[base + d])
    double temp = 0.0;
    for (int d = 0; d < 9; ++d) temp += gf_old[base + d];

    double buoyancy = lattice_buoyancy * (temp - d_room_temp);
    double uyF = 0.0;
    if (density != 0.0) uyF = uy + 0.5 * buoyancy / density;

    for (int d = 0; d < 9; ++d) {
        double cuF = d_cx[d] * ux + d_cy[d] * uyF;
        double forceTerm = d_weights[d] * (1.0 - 0.5 / tauF) *
            (((d_cy[d] - uyF) * buoyancy) / d_cs2 +
             ((d_cx[d] * ux + d_cy[d] * uyF) * (d_cy[d] * buoyancy)) / (d_cs2 * d_cs2));

        double feq = d_weights[d] * density *
            (1.0 + cuF / d_cs2 + (cuF * cuF) / (2.0 * d_cs2 * d_cs2) -
             (ux * ux + uyF * uyF) / (2.0 * d_cs2));

        gf_new[base + 9 + d] = gf_old[base + 9 + d] -
            (1.0 / tauF) * (gf_old[base + 9 + d] - feq) + forceTerm;

        double cuT = d_cx[d] * ux + d_cy[d] * uy;
        double geq = d_weights[d] * temp * (1.0 + cuT / d_cs2);
        gf_new[base + d] = gf_old[base + d] -
            (1.0 / tauT) * (gf_old[base + d] - geq);
    }
}

__global__ void streamKernelAoS(int height, int width,
                                double* gf_new, const double* gf_old,
                                const bool* isRad) {

    int x = blockDim.x * blockIdx.x + threadIdx.x;
    int y = blockDim.y * blockIdx.y + threadIdx.y;
    if (x >= width || y >= height) return;

    int n_cells = width * height;
    int idx = y * width + x;
    int base = idx * STRIDE;

    if (isRad[idx]) return;

    for (int d = 0; d < 9; ++d) {
        int srcX = x - (int)d_cx[d];
        int srcY = y - (int)d_cy[d];

        if (srcX >= 0 && srcY >= 0 && srcX < width && srcY < height) {
            int srcIdx = srcY * width + srcX;
            int srcBase = srcIdx * STRIDE;

            if (isRad[srcIdx]) {
                int opp = d_inv[d];
                double radTemp = 0.0;
                for (int k = 0; k < 9; ++k) radTemp += gf_old[srcBase + k];
                gf_new[base + d] = -gf_old[srcBase + opp] + 2.0 * d_weights[d] * radTemp;
                gf_new[base + 9 + d] = gf_old[srcBase + 9 + opp];
            } else {
                gf_new[base + d] = gf_old[srcBase + d];
                gf_new[base + 9 + d] = gf_old[srcBase + 9 + d];
            }
        } else {
            int opp = d_inv[d];
            gf_new[base + d] = gf_old[base + opp];
            gf_new[base + 9 + d] = gf_old[base + 9 + opp];
        }
    }
}
