cmake_minimum_required(VERSION 3.14)
project(project_2_2_group_1 LANGUAGES CXX)

include(CheckLanguage)
check_language(CUDA)

if(CMAKE_CUDA_COMPILER)
    message("CUDA enabled.")
    enable_language(CUDA)
    add_compile_definitions(CUDA_AVAILABLE=1)
else()
    message("CUDA disabled.")
endif()

# Upgraded to 17. ImGui and WebSockets work better with modern C++ standards
set(CMAKE_CXX_STANDARD 20)

# Disable GCC's default OpenMP offloading — we use native CUDA (nvcc) for GPU.
# Must be set on both compile and link flags; LTO invokes nvptx at link time.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -foffload=disable")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -foffload=disable")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -foffload=disable")

# Option to build with GUI (requires OpenGL/GLFW/ImGui).
# Set -DBUILD_GUI=OFF for headless/batch-only builds (e.g., on a cluster).
option(BUILD_GUI "Build the GUI application" ON)

find_package(Threads REQUIRED)

if(BUILD_GUI)
    find_package(OpenGL REQUIRED)
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

if(BUILD_GUI)
    add_subdirectory(vendor/glfw-3.4)
endif()

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

add_library(simulation_logic
    src/data/SimulationEngine.cpp
    src/data/SimulationEngine.h

    src/data/BatchRunner.cpp
    src/data/BatchRunner.h

    src/data/local/LocalEngine.cpp
    src/data/local/LocalEngine.h

    src/thread/SimulationStateBuffers.cpp
    src/thread/SimulationStateBuffers.h

    src/thread/ReusableThread.cpp
    src/thread/ReusableThread.h

    src/main/main.h
)

if(CMAKE_CUDA_COMPILER)
    set(CUDA_SOURCES
        src/data/local/gpu/CUDA/LocalCUDAEngine.cu
        src/data/local/gpu/CUDA/LocalCUDAEngine.cuh
    )
else()
    set(CUDA_SOURCES "")
endif()

# --- Main executable ---
if(BUILD_GUI)
    if(CMAKE_CUDA_COMPILER)
        set_source_files_properties(src/ui/ui.cpp PROPERTIES LANGUAGE CUDA)
    endif()

    add_executable(project_2_2_group_1
        src/main/main.cpp
        src/ui/ui.cpp
        src/ui/ui.h
        ${IMGUI_SOURCES}
        ${IMPLOT_SOURCES}
        ${CUDA_SOURCES}
    )

    target_link_libraries(project_2_2_group_1
        simulation_logic
        glfw
        OpenGL::GL
        asio
        Threads::Threads
    )

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