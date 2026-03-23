cmake_minimum_required(VERSION 4.1)
project(project_2_2_group_1)

set(CMAKE_CXX_STANDARD 14)

include_directories(src/main)

add_executable(project_2_2_group_1
        src/main/main.cpp
        src/main/main.h)
