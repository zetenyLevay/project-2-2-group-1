# Thermal Lattice Boltzmann Method (LBM) Simulation

**Project 2-2 | Group 1**

## Project Description
This project is a C++ implementation of a 2D Thermal fluid flow simulation using the **Lattice Boltzmann Method (LBM)**. It utilizes a D2Q9 lattice model and a Double-Distribution Function (DDF) approach to simulate both macroscopic fluid momentum and thermodynamic energy transfer.

Currently, this aims to be a robust CPU based prototype. To prepare for High-Performance Computing (HPC) and future GPU/CUDA acceleration, the core data structures are implemented using a **Structure of Arrays (SoA)** memory layout to ensure continuous memory access and maximize memory coalescing.

## Repository Structure
```text
project_2_2_group_1/
├── CMakeLists.txt         # Build configuration
├── README.md              # Project documentation
├── src/
│   ├── main/
│   │   ├── main.cpp       # Main simulation loop and execution
│   │   ├── main.h         # Shared constants, structs, and declarations
│   │   └── lbm.cpp        # Core LBM physics and streaming logic
│   └── ui/
│       ├── ui.cpp         # User interface logic
│       └── ui.h           
└── tests/
    └── test_stream.cpp    # Isolated unit tests for the streaming/boundary module
```

## Prerequisites
To compile and run this project, you will need the following installed on your system:

**C++ Compiler**: Must support C++14 (e.g., GCC, Clang, or MSVC).

**CMake**: Version **4.3**.

## Compilation Instructions
This project uses CMake for an out-of-source build, keeping compiled binaries separate from the source code.

1. Open your terminal and navigate to the root directory of the project.

2. Create a dedicated build directory:
    
    ``` bash 
    mkdir build
    cd build
    ```
3. Generate build files using CMake:
    ``` bash
    cmake -G "MinGW Makefiles" ..
    ```
4. Compile the project:
    ``` bash
    cmake --build .
    ```

## Running the Simulation
Once compiled the main executable is in your build directory.

To run the main simulation loop:

Linux / macOS: ./project_2_2_group_1

Windows: Debug\project_2_2_group_1.exe (or just project_2_2_group_1.exe depending on your compiler)

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