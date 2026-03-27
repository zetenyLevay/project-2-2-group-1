cmake_minimum_required(VERSION 3.2)
project(project_2_2_group_1)

set(CMAKE_CXX_STANDARD 14)

include_directories(src/main)
include_directories(src/ui)

find_package(Threads REQUIRED)

include(FetchContent)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-28-0
)
FetchContent_MakeAvailable(asio)
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)
FetchContent_Declare(
    websocketpp
    GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
    GIT_TAG        0.8.2
)
FetchContent_MakeAvailable(websocketpp)

include_directories(${websocketpp_SOURCE_DIR})

add_library(simulation_logic
        src/main/lbm.cpp
        src/main/main.h
        src/ui/ui.cpp
        src/ui/ui.h
)

add_executable(project_2_2_group_1 src/main/main.cpp)
target_link_libraries(project_2_2_group_1 simulation_logic asio Threads::Threads)

enable_testing()

add_executable(test_stream tests/test_stream.cpp)
target_link_libraries(test_stream simulation_logic asio Threads::Threads)

add_test(NAME StreamAndBoundaryTest COMMAND test_stream)