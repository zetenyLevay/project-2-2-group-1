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

find_package(Threads REQUIRED) # For WebSocket background threading
find_package(OpenGL REQUIRED)  # For ImGui hardware acceleration

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

add_subdirectory(vendor/glfw-3.4)

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

include_directories(
    src/main
    src/ui
    src/physics
    vendor/imgui
    vendor/imgui/backends
    vendor/implot
    vendor/pfd
)

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

add_executable(project_2_2_group_1
    src/main/main.cpp
    src/ui/ui.cpp
    src/ui/ui.h
    ${IMGUI_SOURCES}
    ${IMPLOT_SOURCES}
    ${CUDA_SOURCES}
)

# Link everything together for the main app
target_link_libraries(project_2_2_group_1 
    simulation_logic 
    glfw 
    OpenGL::GL 
    asio
    Threads::Threads
)