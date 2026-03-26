cmake_minimum_required(VERSION 4.1)
project(project_2_2_group_1)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_PREFIX_PATH "/opt/homebrew")


find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)



add_executable(project_2_2_group_1
        src/main/main.cpp
        src/main/main.h
        src/ui/ui.cpp
        src/ui/ui.h
        src/opengl/main.cpp
        src/opengl/main.h
        src/opengl/helloworld.cpp)


target_link_libraries(
        project_2_2_group_1 PRIVATE
        glfw
        OpenGL::GL
)