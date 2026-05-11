cmake_minimum_required(VERSION 3.14)
project(project_2_2_group_1 LANGUAGES CXX)

include(CheckLanguage)
check_language(CUDA)

if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    add_compile_definitions(CUDA_AVAILABLE=1)
endif()

# Upgraded to 17. ImGui and WebSockets work better with modern C++ standards
set(CMAKE_CXX_STANDARD 17)

# Option to build with GUI (requires OpenGL/GLFW/ImGui).
# Set -DBUILD_GUI=OFF for headless/batch-only builds (e.g., on a cluster).
option(BUILD_GUI "Build the GUI application" ON)

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
        add_compile_definitions(USE_OMP_TARGET_OFFLOAD)
        message(STATUS "OpenMP target offloading FORCE-ENABLED: ${OMP_OFFLOAD_TARGET}")
    else()
        include(CheckCXXSourceCompiles)
        set(CMAKE_REQUIRED_FLAGS "-fopenmp -foffload=${OMP_OFFLOAD_TARGET}")
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
            add_compile_definitions(USE_OMP_TARGET_OFFLOAD)
            message(STATUS "OpenMP target offloading enabled: ${OMP_OFFLOAD_TARGET}")
        else()
            message(STATUS "OpenMP target offloading not available — using CPU-only OpenMP")
        endif()
        unset(CMAKE_REQUIRED_FLAGS)
    endif()
else()
    message(WARNING "OpenMP not found, compilation will proceed without parallelization")
endif()

find_package(Threads REQUIRED) # For WebSocket and batch threading

# GUI dependencies (OpenGL, GLFW)
if(BUILD_GUI)
    message(STATUS "BUILD_GUI=ON — looking for OpenGL")
    find_package(OpenGL REQUIRED)  # For ImGui hardware acceleration
    add_subdirectory(vendor/glfw-3.4)
else()
    message(STATUS "BUILD_GUI=OFF — skipping OpenGL/GLFW dependencies")
endif()

include(FetchContent)

# Asio
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-28-0
)
FetchContent_MakeAvailable(asio)
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)

# WebSocket++
FetchContent_Declare(
    websocketpp
    GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
    GIT_TAG        0.8.2
    # Deletes the outdated CMakeLists.txt to make it compatible with cmake 4.3
    PATCH_COMMAND  ${CMAKE_COMMAND} -E rm -f CMakeLists.txt
)
FetchContent_MakeAvailable(websocketpp)

# Force C++11 to bypass boost
add_library(websocketpp INTERFACE)
target_include_directories(websocketpp INTERFACE ${websocketpp_SOURCE_DIR})
target_compile_definitions(websocketpp INTERFACE
    ASIO_STANDALONE
    _WEBSOCKETPP_CPP11_STL_
    _WEBSOCKETPP_CPP11_THREAD_
    _WEBSOCKETPP_CPP11_FUNCTIONAL_
    _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
    _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
    _WEBSOCKETPP_CPP11_MEMORY_
)

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

    src/thread/ReusableThread.cpp
    src/thread/ReusableThread.h

    src/main/main.h
)

# --- Main executables ---
if(BUILD_GUI)
    add_executable(project_2_2_group_1
        src/main/main.cpp
        src/ui/ui.cpp
        src/ui/ui.h
        ${IMGUI_SOURCES}
        ${IMPLOT_SOURCES}
    )

    target_link_libraries(project_2_2_group_1
        simulation_logic
        glfw
        OpenGL::GL
        asio
        websocketpp
        Threads::Threads
    )

    # Windows-specific networking libraries
    if(WIN32)
        target_link_libraries(project_2_2_group_1 ws2_32 wsock32)
    endif()
else()
    add_executable(project_2_2_group_1
        src/main/main_batch.cpp
    )

    target_link_libraries(project_2_2_group_1
        simulation_logic
        Threads::Threads
    )

    if(OpenMP_CXX_FOUND)
        target_link_libraries(project_2_2_group_1 OpenMP::OpenMP_CXX)
    endif()
endif()

# Testing
enable_testing()
add_executable(test_stream tests/test_stream.cpp)
# The test only links the math and network, skipping the heavy UI stuff
target_link_libraries(test_stream simulation_logic asio websocketpp Threads::Threads)

# Windows-specific networking libraries for tests
if(WIN32)
    target_link_libraries(test_stream ws2_32 wsock32)
endif()
add_test(NAME StreamAndBoundaryTest COMMAND test_stream)

# Benchmark executable
add_executable(benchmark_lbm benchmark_lbm.cpp)
target_link_libraries(benchmark_lbm simulation_logic asio websocketpp Threads::Threads)
if(OpenMP_CXX_FOUND)
    target_link_libraries(benchmark_lbm OpenMP::OpenMP_CXX)
endif()

if(WIN32)
    target_link_libraries(benchmark_lbm ws2_32 wsock32)
endif()



if(CMAKE_CUDA_COMPILER)
    add_executable(CollisionTest
        src/CUDA/CollisionTest.cpp
        src/CUDA/CollisionCUDA.cu
        src/data/local/LocalEngine.cpp
        src/data/SimulationEngine.cpp
        src/thread/ReusableThread.cpp

    )

    set_target_properties(CollisionTest PROPERTIES
        CUDA_STANDARD 17
    )
endif()