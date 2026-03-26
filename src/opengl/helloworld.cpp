//
// Created by Ilya Lisenco on 26/03/2026.
//
#include <GLFW/glfw3.h>
#include <iostream>

int main() {
//initialize glfw
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }


    //create the window
    GLFWwindow* window = glfwCreateWindow(800, 600, "GLFW Test Window", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }

    //make opengl contezt current
    glfwMakeContextCurrent(window);

    //main loop
    while (!glfwWindowShouldClose(window)) {
        //just a black screen
        glClear(GL_COLOR_BUFFER_BIT);

        //swap buffers for opengl to draw the frames and display them
        glfwSwapBuffers(window);

        //window should be totally irresponsive without this
        glfwPollEvents();
    }

    //clean
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}