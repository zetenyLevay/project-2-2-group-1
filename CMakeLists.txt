cmake_minimum_required(VERSION 3.2)
project(project_2_2_group_1)

set(CMAKE_CXX_STANDARD 14)

include_directories(src/main)
include_directories(src/ui)

add_library(simulation_logic
        src/main/lbm.cpp
        src/main/main.h
        src/ui/ui.cpp
        src/ui/ui.h
)

add_executable(project_2_2_group_1 src/main/main.cpp)
target_link_libraries(project_2_2_group_1 simulation_logic)

enable_testing()

add_executable(test_stream tests/test_stream.cpp)
target_link_libraries(test_stream simulation_logic)

add_test(NAME StreamAndBoundaryTest COMMAND test_stream)