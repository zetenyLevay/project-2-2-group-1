# Thermal Lattice Boltzmann Method (LBM) Simulation

**Project 2-2 | Group 1**

## Project Description
This project is a C++ implementation of a 2D Thermal fluid flow simulation using the **Lattice Boltzmann Method (LBM)**. It utilizes a D2Q9 lattice model and a Double-Distribution Function (DDF) approach to simulate both macroscopic fluid momentum and thermodynamic energy transfer.

Currently, this aims to be a robust CPU based prototype. To prepare for High-Performance Computing (HPC) and future GPU/CUDA acceleration, the core data structures are implemented using a **Structure of Arrays (SoA)** memory layout to ensure continuous memory access and maximize memory coalescing.

.

## Prerequisites

- **C++17** compiler (GCC 11+, Clang 14+)
- **CMake** 3.14+
- **CUDA Toolkit** 12+ (optional, for GPU acceleration)
- **OpenGL** + **GLFW** (optional, for GUI)
- **OpenMP** (optional, for CPU multi-threading)
- **Python 3** + matplotlib (optional, for plotting cavity results)

## Compilation

```bash
# Headless CPU-only (serial)
cmake -DBUILD_GUI=OFF -DCMAKE_CUDA_COMPILER="" .. -B build-cpu
cmake --build build-cpu

# Headless with CUDA GPU acceleration
cmake -DBUILD_GUI=OFF .. -B build-cuda
cmake --build build-cuda

# GUI (with CUDA if available, falls back to CPU)
cmake -DBUILD_GUI=ON .. -B build-gui
cmake --build build-gui
./build-gui/project_2_2_group_1

# Benchmark tool
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
| CPU OpenMP | `LocalEngine` | — | Multi-core via `#pragma omp parallel for` |
| GPU CUDA SoA | `LocalCUDAEngine` | NVIDIA | Hand-written CUDA kernels |
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

Validates the isothermal LBM solver against the classic Ghia et al. benchmark. Uses Zou/He boundary conditions on the lid and bounce-back on walls. Exports velocity profiles to CSV for comparison.

```bash
./build/project_2_2_group_1 --cavity 128 100 0.1   # N=128, Re=100, U_lid=0.1
```

## Key optimizations

| Optimization | Impact |
|-------------|--------|
| GPU-resident grids (no per-step H↔D transfer) | 288 MB/step → 0 |
| Batch mode scalar-only download | 2 MB → 24 bytes/step |
| CSR view factor grouping | Radiation: 25 ms CPU → 0.4 ms GPU (62×) |
| Pinned host memory (`cudaMallocHost`) | ~3× faster D2H transfers |
| SoA memory layout | 30% faster than AoS at ≥256² |

## Performance (L40 cluster, 4096², full physics)

| GPUs | MLUPS | Steps/sec |
|------|-------|-----------|
| 1 | 288 | 17.2 |
| 2 | 577 | 34.4 |
| 3 | 866 | 51.6 |
| 4 | 1,153 | 68.7 |

## Cluster (SLURM)

```bash
sbatch scripts/slurm_benchmark.sh
```

The SLURM script auto-detects the GPU architecture (L40/A100/H100) and sets the correct CUDA compile target.

## Research questions

| RQ | Question | Status |
|----|----------|--------|
| RQ1 | GPU scaling (strong/weak) | Single-GPU ✅, Multi-GPU ✅ (perfect linear to 4 GPUs) |
| RQ2 | Diminishing returns | No diminishing returns up to 4 GPUs |
| RQ3 | AoS vs SoA memory layout | SoA 30% faster at ≥256² |
| RQ4 | GPU vs CPU speedup | 17–45× over CPU serial, 9–17× over OpenMP |
