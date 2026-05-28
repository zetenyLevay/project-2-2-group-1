# Thermal Lattice Boltzmann Method (LBM) Simulation

**Project 2-2 | Group 1**

## Project Description
This project is a C++ implementation of a 2D Thermal fluid flow simulation using the **Lattice Boltzmann Method (LBM)**. It utilizes a D2Q9 lattice model and a Double-Distribution Function (DDF) approach to simulate both macroscopic fluid momentum and thermodynamic energy transfer.

Currently, this aims to be a robust CPU based prototype. To prepare for High-Performance Computing (HPC) and future GPU/CUDA acceleration, the core data structures are implemented using a **Structure of Arrays (SoA)** memory layout to ensure continuous memory access and maximize memory coalescing.

## Repository Structure
```text
project_2_2_group_1/
├── CMakeLists.txt                  # Build configuration
├── README.md                       # Project documentation
├── benchmark_lbm.cpp               # OpenMP benchmark tool
├── src/
│   ├── main/
│   │   ├── main.cpp                # GUI entry point (WebSocket + UI)
│   │   ├── main_batch.cpp          # Batch-only entry point (headless)
│   │   └── main.h                  # Shared constants, structs, and declarations
│   ├── ui/
│   │   ├── ui.cpp                  # User interface logic
│   │   └── ui.h  
│   ├── thread/   
│   │   ├── ReusableThread.cpp      # Thread logic         
│   │   └── ReusableThread.h     
│   └── data/   
│       ├── SimulationEngine.cpp    # Base simulation engine       
│       ├── SimulationEngine.h  
│       ├── BatchRunner.cpp         # Batch simulation runner
│       ├── BatchRunner.h    
│       └── local/
│           ├── LocalEngine.cpp     # LBM physics (collision, streaming)
│           └── LocalEngine.h    
├── tests/
│   └── test_stream.cpp             # Unit tests for streaming
├── saves/                          # Simulation save files (generated)
└── vendor/                         # Third-party dependencies
    ├── glfw-3.4/
    ├── imgui/
    ├── implot/
    └── pfd/
```

## Prerequisites
To compile and run this project, you will need the following installed on your system:

**C++ Compiler**: Must support **C++17** (e.g., GCC, Clang, or MSVC).

**CMake**: Version **3.5 - 4.3**.

## Compilation Instructions
This project uses CMake for an out-of-source build, keeping compiled binaries separate from the source code.

### GUI Build (default)
Builds the full desktop application with heatmap visualization, controls, and WebSocket server.

``` bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./project_2_2_group_1
```

### Batch-Only Build (No GUI)
For running simulations on a cluster or headless system without OpenGL/display dependencies.

``` bash
mkdir build && cd build
cmake .. -DBUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build .
./project_2_2_group_1 --batch <width> <height> <numberOfSims> <filename> <saveType>
```

### OpenMP 5.0 GPU Offloading (cluster with NVIDIA GPU)
If your system has GCC with `nvptx-tools` installed, offloading is auto-detected.
Otherwise, force-enable it for cluster builds:

``` bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=OFF -DENABLE_OMP_OFFLOAD=ON -DOMP_OFFLOAD_TARGET=nvptx-none -DCMAKE_CXX_FLAGS="-fcf-protection=none -fno-stack-protector -DUSE_OMP_TARGET_OFFLOAD"
cmake --build .
```

(Use `-DOMP_OFFLOAD_TARGET=amdgcn-amd-amdhsa` for AMD GPUs.)

## Running the Simulation

### GUI Mode (default build)
``` bash
./project_2_2_group_1
```
Launches a desktop window with a heatmap visualization, simulation controls (play/pause, step, timeline), temperature convergence graph, and save/load functionality.

### Batch Mode
Runs simulations to equilibrium without any display, useful for data collection or cluster execution.

``` bash
./project_2_2_group_1 --batch <width> <height> <numberOfSims> <filename> <saveType>
```

| Argument | Description |
|---|---|
| `width`, `height` | Grid dimensions in cells |
| `numberOfSims` | Number of independent simulations to run |
| `filename` | Output file base name (saved to `saves/`) |
| `saveType` | `0` = Necessary (temperatures only), `1` = Complete (full grid state) |

### Lid-Driven Cavity Benchmark (physics validation)
Runs an isothermal lid-driven cavity simulation to steady state and exports centerline velocity profiles to CSV. This validates the LBM physics implementation against the reference data from **Ghia et al. (1982)**.

``` bash
./project_2_2_group_1 --cavity              # N=64, Re=100, U_lid=0.1
./project_2_2_group_1 --cavity 128          # 128x128 grid
./project_2_2_group_1 --cavity 64 400       # Re=400
./project_2_2_group_1 --cavity 64 100 0.05  # Custom lid velocity
```

| Argument | Description | Default |
|---|---|---|
| `N` | Grid size (N×N cells) | `64` |
| `Re` | Reynolds number | `100.0` |
| `U_lid` | Lid velocity (lattice units) | `0.1` |

Output is saved to `saves/cavity_Re<N>_N<N>.csv` containing vertical and horizontal centerline velocity profiles. Compare these against Ghia et al. (1982) data to verify correctness.

### OpenMP Performance Benchmark (speed measurement)
Measures performance across grid sizes with and without OpenMP. This tests **throughput** (steps/second, cells/second), not physics correctness.

``` bash
./benchmark_lbm                    # Both sequential and OpenMP
./benchmark_lbm --sequential       # CPU only
./benchmark_lbm --openmp           # OpenMP only
./benchmark_lbm --steps 200       # Custom step count
OMP_NUM_THREADS=4 ./benchmark_lbm # Set thread count
```

| Flag | Description |
|---|---|
| *(none)* | Runs both sequential (1 thread) and parallel (all cores) on 50×50, 100×100, and 200×200 grids |
| `--sequential` | Sequential only |
| `--openmp` | OpenMP only (uses `OMP_NUM_THREADS` or all available cores) |
| `--steps N` | Number of simulation steps per benchmark run (default: 100) |
| `OMP_NUM_THREADS=N` | Environment variable to control thread count |

> **Key difference**: The lid-driven cavity benchmark (`--cavity`) validates **physics correctness** by checking convergence against a known CFD problem. The OpenMP benchmark (`benchmark_lbm`) measures **execution speed** and parallel scaling, it uses the same LocalEngine but reports time, throughput, and speedup rather than flow profiles.


## Testing
We use CMake's default testing framework (CTest) to validate individual modules without running the full simulation loop.

To run the tests ensure you are inside the build directory and run:

```bash
ctest --output-on-failure
```
Note: The ```text --output-on-failure``` flag ensures that if a test (like test_stream) fails an assertion, the terminal will print exactly which line of code caused the crash.

Alternatively, you can run the test executable manually to see its terminal output:

Unix: ./test_stream

Windows: Debug\test_stream.exe