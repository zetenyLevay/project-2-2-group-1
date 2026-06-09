#pragma once

#ifdef __CUDACC__

extern __constant__ double d_cx[9];
extern __constant__ double d_cy[9];
extern __constant__ double d_weights[9];
extern __constant__ double d_cs2;
extern __constant__ int    d_inv[9];
extern __constant__ double d_room_temp;
extern __constant__ double d_max_temp;

__device__ double atomicMaxD(double* addr, double val);
__device__ double atomicMinD(double* addr, double val);
__device__ double atomicAddD(double* addr, double val);

#endif
