cmake_minimum_required(VERSION 3.14)
project(project_2_2_group_1 LANGUAGES CXX)

include(CheckLanguage)
check_language(CUDA)

# Prevent GCC-injected -fopenmp from triggering nvptx offload (EESSI toolchain).
# Only when NOT explicitly building for OpenMP offload.
# Must be on both compile and link to stop LTO nvptx invocations.
if(NOT ENABLE_OMP_OFFLOAD)
    add_compile_options(-foffload=disable)
    add_link_options(-foffload=disable)
endif()

if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    add_compile_definitions(CUDA_AVAILABLE=1)
    set(CMAKE_CUDA_ARCHITECTURES 89)
endif()

# Upgraded to 17. ImGui and WebSockets work better with modern C++ standards
set(CMAKE_CXX_STANDARD 17)

# Option to build with GUI (requires OpenGL/GLFW/ImGui).
# Set -DBUILD_GUI=OFF for headless/batch-only builds (e.g., on a cluster).
option(BUILD_GUI "Build the GUI application" ON)

find_package(Threads REQUIRED)

# OpenMP support for parallelization
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found, enabling parallelization")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

    # OpenMP 5.0 target offloading (CPU+GPU with same pragmas)
    # Auto detected by default
    # Override with -DENABLE_OMP_OFFLOAD=ON for cluster builds.
    option(ENABLE_OMP_OFFLOAD "Force-enable OpenMP target offloading (use for cluster builds)" OFF)
    set(OMP_OFFLOAD_TARGET "nvptx-none" CACHE STRING "OpenMP offload target (e.g. nvptx-none, amdgcn-amd-amdhsa)")

    if(ENABLE_OMP_OFFLOAD)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -foffload=${OMP_OFFLOAD_TARGET}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -foffload-options=${OMP_OFFLOAD_TARGET}=-fcf-protection=none")
        add_compile_definitions(USE_OMP_TARGET_OFFLOAD)
        message(STATUS "OpenMP target offloading FORCE-ENABLED: ${OMP_OFFLOAD_TARGET}")
    else()
        include(CheckCXXSourceCompiles)
        set(CMAKE_REQUIRED_FLAGS "-fopenmp")
        check_cxx_source_compiles("
            int main() {
                int x = 0;
                #pragma omp target map(from: x)
                x = 42;
                return x - 42;
            }
        " HAS_OMP_OFFLOAD)
        if(HAS_OMP_OFFLOAD)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -foffload=${OMP_OFFLOAD_TARGET}")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -foffload-options=${OMP_OFFLOAD_TARGET}=-fcf-protection=none")
            add_compile_definitions(USE_OMP_TARGET_OFFLOAD)
            message(STATUS "OpenMP target offloading enabled: ${OMP_OFFLOAD_TARGET}")
        else()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
            message(STATUS "OpenMP target offloading not available — using CPU-only OpenMP")
        endif()
        unset(CMAKE_REQUIRED_FLAGS)
    endif()
else()
    message(WARNING "OpenMP not found, compilation will proceed without parallelization")
endif()

# GUI dependencies (OpenGL, GLFW)
if(BUILD_GUI)
    message(STATUS "BUILD_GUI=ON — looking for OpenGL")
    find_package(OpenGL REQUIRED)  # For ImGui hardware acceleration
    add_subdirectory(vendor/glfw-3.4)
else()
    message(STATUS "BUILD_GUI=OFF — skipping OpenGL/GLFW dependencies")
endif()

# GUI source files (ImGui, ImPlot)
if(BUILD_GUI)
    set(IMGUI_SOURCES
        vendor/imgui/imgui.cpp
        vendor/imgui/imgui_draw.cpp
        vendor/imgui/imgui_widgets.cpp
        vendor/imgui/imgui_tables.cpp
        vendor/imgui/imgui_demo.cpp
        vendor/imgui/backends/imgui_impl_glfw.cpp
        vendor/imgui/backends/imgui_impl_opengl3.cpp
    )

    set(IMPLOT_SOURCES
        vendor/implot/implot.cpp
        vendor/implot/implot_items.cpp
        vendor/implot/implot_demo.cpp
    )
endif()

# Include directories
include_directories(
    src/main
    src/data
    src/data/local
    src/data/cavity
    src/thread
)

if(BUILD_GUI)
    include_directories(
        src/ui
        vendor/imgui
        vendor/imgui/backends
        vendor/implot
        vendor/pfd
    )
endif()

# Core simulation library (shared between GUI and batch builds)
add_library(simulation_logic
    src/data/SimulationEngine.cpp
    src/data/SimulationEngine.h

    src/data/BatchRunner.cpp
    src/data/BatchRunner.h

    src/data/local/LocalEngine.cpp
    src/data/local/LocalEngine.h

    src/data/cavity/LidDrivenCavity.cpp
    src/data/cavity/LidDrivenCavity.h

    src/thread/SimulationStateBuffers.cpp
    src/thread/SimulationStateBuffers.h

    src/thread/ReusableThread.cpp
    src/thread/ReusableThread.h

    src/main/main.h
)

# CUDA sources (only when CUDA compiler is available)
if(CMAKE_CUDA_COMPILER)
    set(CUDA_SOURCES
        src/data/local/gpu/CUDA/kernels.cu
        src/data/local/gpu/CUDA/kernels.cuh
        src/data/local/gpu/CUDA/LocalCUDAEngine.cu
        src/data/local/gpu/CUDA/LocalCUDAEngine.cuh
        src/data/local/gpu/CUDA/kernels_aos.cu
        src/data/local/gpu/CUDA/kernels_aos.cuh
        src/data/local/gpu/CUDA/LocalCUDAEngineAoS.cu
        src/data/local/gpu/CUDA/LocalCUDAEngineAoS.cuh
    )
else()
    set(CUDA_SOURCES "")
endif()

# --- Main executables ---
if(BUILD_GUI)
    if(CMAKE_CUDA_COMPILER)
        set_source_files_properties(src/ui/ui.cpp PROPERTIES LANGUAGE CUDA)
    endif()

    add_executable(project_2_2_group_1
        src/main/main.cpp
        src/ui/ui.cpp
        ${CUDA_SOURCES}
        src/ui/ui.h
        ${IMGUI_SOURCES}
        ${IMPLOT_SOURCES}
    )

    target_link_libraries(project_2_2_group_1
        simulation_logic
        glfw
        OpenGL::GL
    )

    # Windows-specific networking libraries
    if(WIN32)
        target_link_libraries(project_2_2_group_1 ws2_32 wsock32)
    endif()
else()
    add_executable(project_2_2_group_1
        src/main/main_batch.cpp
        ${CUDA_SOURCES}
    )

    target_link_libraries(project_2_2_group_1
        simulation_logic
        Threads::Threads
    )
endif()

if(OpenMP_CXX_FOUND)
    target_link_libraries(project_2_2_group_1 OpenMP::OpenMP_CXX)
endif()

# Testing
enable_testing()
add_executable(test_stream tests/test_stream.cpp)
# The test only links the math and network, skipping the heavy UI stuff
target_link_libraries(test_stream simulation_logic)

# Windows-specific networking libraries for tests
if(WIN32)
    target_link_libraries(test_stream ws2_32 wsock32)
endif()
add_test(NAME StreamAndBoundaryTest COMMAND test_stream)

# Benchmark executable
add_executable(benchmark_lbm benchmark_lbm.cpp ${CUDA_SOURCES})
target_link_libraries(benchmark_lbm simulation_logic)
if(OpenMP_CXX_FOUND)
    target_link_libraries(benchmark_lbm OpenMP::OpenMP_CXX)
endif()

if(WIN32)
    target_link_libraries(benchmark_lbm ws2_32 wsock32)
endif()



# CollisionTest disabled on main — requires CUDA branch Grid layout (array-of-vectors)
# which is incompatible with main's flat-vector SoA layout.
# if(CMAKE_CUDA_COMPILER)
#     add_executable(CollisionTest
#         src/CUDA/CollisionTest.cpp
#         src/CUDA/CollisionCUDA.cu
#         src/data/local/LocalEngine.cpp
#         src/data/SimulationEngine.cpp
#         src/thread/ReusableThread.cpp
#     )
#     set_target_properties(CollisionTest PROPERTIES
#         CUDA_STANDARD 17
#     )
# endif()