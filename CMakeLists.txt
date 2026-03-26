cmake_minimum_required(VERSION 4.1)
project(project_2_2_group_1)

set(CMAKE_CXX_STANDARD 14)

set(IMGUI_SOURCES
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_demo.cpp
    vendor/imgui/backends/imgui_impl_glfw.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
)

add_executable(project_2_2_group_1
        src/main/main.cpp
        src/main/main.h

        src/ui/ui.cpp
        src/ui/ui.h

        ${IMGUI_SOURCES}
)

target_include_directories(project_2_2_group_1 PRIVATE src/main)
target_include_directories(project_2_2_group_1 PRIVATE vendor/imgui)
target_include_directories(project_2_2_group_1 PRIVATE vendor/imgui/backends)

add_subdirectory(vendor/glfw-3.4)

target_link_libraries(project_2_2_group_1 glfw)

find_package(OpenGL REQUIRED)

target_link_libraries(project_2_2_group_1 OpenGL::GL)
