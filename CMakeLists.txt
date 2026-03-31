cmake_minimum_required(VERSION 3.14)
project(project_2_2_group_1)

# Upgraded to 17. ImGui and WebSockets work better with modern C++ standards
set(CMAKE_CXX_STANDARD 17) 

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
)

add_library(simulation_logic
    src/physics/lbm.cpp
    src/physics/SimulationEngine.cpp
    src/physics/SimulationEngine.h
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
    ws2_32   # Required for MinGW / Windows networking
    wsock32  # Required for MinGW / Windows networking
)

# Testing
enable_testing()
add_executable(test_stream tests/test_stream.cpp)
# The test only links the math and network, skipping the heavy UI stuff
target_link_libraries(test_stream simulation_logic asio websocketpp Threads::Threads ws2_32 wsock32)
add_test(NAME StreamAndBoundaryTest COMMAND test_stream)