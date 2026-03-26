cmake_minimum_required(VERSION 4.1)
project(project_2_2_group_1)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_PREFIX_PATH "/opt/homebrew")

find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)

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

add_executable(project_2_2_group_1
        src/main/main.cpp
        src/main/main.h
        src/ui/ui.cpp
        src/ui/ui.h
        src/opengl/main.cpp
        src/opengl/main.h
        src/opengl/helloworld.cpp
        vendor/glad/src/glad.c

        ${IMGUI_SOURCES}
        ${IMPLOT_SOURCES}
)

target_include_directories(project_2_2_group_1 PRIVATE
        src/main
        vendor/imgui
        vendor/imgui/backends
        vendor/implot
        vendor/glad/include
        src/opengl
)

target_link_libraries(project_2_2_group_1 PRIVATE
        glfw
        OpenGL::GL
)