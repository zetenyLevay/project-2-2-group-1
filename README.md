# Thermal Lattice Boltzmann Method (LBM) Simulation

**Project 2-2 | Group 1**

## Project Description
This project is a C++/CUDA implementation of a 2D Thermal fluid flow simulation using the **Lattice Boltzmann Method (LBM)**. It utilizes a D2Q9 lattice model and a Double-Distribution Function (DDF) approach to simulate both macroscopic fluid momentum and thermodynamic energy transfer.

This project implements both a C++ based parallelized CPU implementation using OpenMP and an optimized CUDA implementation. The results are visualized by a GUI implementation by default unless another option is specified as a parameter.

In order to fully optimize the GPU implementation, a Structure of Arrays (SoA) memory layout is used to maximize memory performance. More details are available in our report.

The GUI will use CUDA implementation if compiled on a machine with CUDA support, otherwise it will fallback to the CPU implementation.

## Prerequisites

- **C++17** compiler
- **CMake** 3.15+
- **CUDA Toolkit** 12+ (optional, for GPU acceleration)
- **OpenGL** + **GLFW** (optional, for GUI)
- **OpenMP** (optional, for CPU multi-threading)
- **Python 3** + matplotlib (optional, for plotting cavity results)

## Compilation

```bash
# Headless CPU-only (serial)
mkdir build && cd build
cmake -DBUILD_GUI=OFF -DCMAKE_CUDA_COMPILER="" .. -B build-cpu
cmake --build build-cpu

# Headless with CUDA GPU acceleration
mkdir build && cd build
cmake -DBUILD_GUI=OFF .. -B build-cuda
cmake --build build-cuda

# GUI (with CUDA if available, falls back to CPU)
mkdir build && cd build
cmake -DBUILD_GUI=ON .. -B build-gui
cmake --build build-gui
./build-gui/project_2_2_group_1

# Benchmark tool
mkdir build && cd build
cmake -DBUILD_GUI=OFF .. -B build-cuda
cmake --build build-cuda --target benchmark_lbm
./build-cuda/benchmark_lbm --engine cuda --width 1024 --height 1024 --steps 100 --csv

# Full benchmark sweep
./scripts/benchmark_script.sh --cluster > results.csv
```

## Batch mode (headless simulation)

```bash
# Run a simulation to thermal steady state (mean > 30°C)
./build-cuda/project_2_2_group_1 --batch 512 512 55 true 1 my_simulation
# Arguments: width height temperature constantHeat numberOfSims filename
```

## Execution modes

| Mode | Engine | GPU | Parallelism |
|------|--------|-----|-------------|
| CPU serial | `LocalEngine` | — | Single thread |
| CPU OpenMP | `LocalEngine` | — | Multi-core using OpenMP |
| GPU CUDA SoA | `LocalCUDAEngine` | NVIDIA | CUDA kernels |
| GPU CUDA AoS | `LocalCUDAEngineAoS` | NVIDIA | Uncoalesced layout (RQ3 comparison) |

## Repository structure

```
project_2_2_group_1/
├── CMakeLists.txt
├── README.md
├── benchmark_lbm.cpp               # Multi-mode benchmark
├── scripts/
│   ├── benchmark_script.sh         # Automated grid sweep
├── src/
│   ├── main/
│   │   ├── main.cpp                # GUI entry point
│   │   ├── main_batch.cpp          # Headless batch entry point
│   │   └── main.h                  # Constants, Grid struct
│   ├── ui/
│   │   ├── ui.cpp / ui.h           # ImGui + ImPlot GUI
│   ├── thread/
│   │   ├── ReusableThread.cpp/.h   # Triple-buffered compute thread
│   │   └── SimulationStateBuffers.cpp/.h
│   ├── data/
│   │   ├── SimulationEngine.cpp/.h # Base engine, SimulationState
│   │   ├── BatchRunner.cpp/.h      # Batch simulation runner
│   │   ├── cavity/                 # Lid-driven cavity benchmark
│   │   └── local/
│   │       ├── LocalEngine.cpp/.h  # CPU LBM implementation
│   │       └── gpu/CUDA/
│   │           ├── kernels.cu/.cuh           # GPU kernels (SoA)
│   │           ├── kernels_aos.cu/.cuh       # GPU kernels (AoS)
│   │           ├── kernels_shared.cuh        # Shared constant memory
│   │           ├── LocalCUDAEngine.cu/.cuh   # CUDA engine (SoA)
│   │           └── LocalCUDAEngineAoS.cu/.cuh # CUDA engine (AoS)
└── tests/
    └── test_stream.cpp
```

## Lid-Driven Cavity Benchmark (physics validation)

Validates the isothermal LBM solver against the Ghia et al. (1982) benchmark. Uses Zou/He boundary conditions on the lid and bounce-back on walls. Exports velocity profiles to CSV for comparison.

```bash
./build/project_2_2_group_1 --cavity 128 100 0.1   # N=128, Re=100, U_lid=0.1
```

## Key optimizations

| Optimization | Impact |
|-------------|--------|
| GPU-resident grids (Host-Device memory transfers greatly reduced) | 288 MB/step -> 0 bytes/step |
| Batch mode scalar-only download | 2 MB/step -> 24 bytes/step |
| CSR view factor grouping | Radiation: 25 ms/step CPU -> 0.4 ms/step GPU (factor of 62×) |
| Pinned host memory (`cudaMallocHost`) | ~3× faster Device to Host transfers |
| SoA memory layout | 30% faster than AoS for grids larger than 256x256 |

## Performance (L40 cluster, 4096x4096 grid resolution, full physics)

| GPUs | Millions of Lattice Updates per Second (MLUPS) | Steps/s |
|------|-------|-----------|
| 1 | 288 | 17.2 |
| 2 | 577 | 34.4 |
| 3 | 866 | 51.6 |
| 4 | 1,153 | 68.7 |