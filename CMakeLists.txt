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

add_executable(project_2_2_group_1
    src/main/main.cpp
    src/ui/ui.cpp
    src/ui/ui.h
    ${IMGUI_SOURCES}
    ${IMPLOT_SOURCES}
)

# Link everything together for the main app
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


if(CMAKE_CUDA_COMPILER)
    add_executable(
        src/data/local/gpu/CUDA/LocalCUDAEngine.cu
        src/data/local/gpu/CUDA/LocalCUDAEngine.cuh
    )

    set_target_properties(CudaBenchmark PROPERTIES
        CUDA_STANDARD 17
    )

    target_include_directories(CudaBenchmark PRIVATE src/main)
endif()