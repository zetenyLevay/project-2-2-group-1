#include <cstdio>
#include <cuda_runtime.h>

__global__ void helloKernel() {
    printf("Hello from GPU!\n");
}

void runHelloWorld() {
    helloKernel << <1, 1 >> > ();
    cudaDeviceSynchronize();
}