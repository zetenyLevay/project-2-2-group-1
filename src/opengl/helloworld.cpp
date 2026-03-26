#include <glad/glad.h> //helps us map the pointers to GPU commands (like a look-up table we have in OS for sys-calls, but for GPU commands
#include <GLFW/glfw3.h> //helps us manage the process of asking the OS to create a window, switch context to window, interract with it, etc
#include <iostream>

//this function changes the size of the GL rendering context
void framebuffer_size_callback(GLFWwindow* window, int width, int height){
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}


int main() {


    //initialize glfw
    glfwInit();

    // some system hints for the glfw, like what version should it use for glfw/opengl (as i understood)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    //create a GLFW window (800x600), null is for monitor and share.
    GLFWwindow* window = glfwCreateWindow(800, 600, "LearnOpenGL", NULL, NULL);

    //report error if window could not have been created
    if (window == NULL) {
        std::cout<<"Failed to create GLFW window."<<std::endl;
        glfwTerminate();
        return -1;
    }

    //make the window the main context of the current thread
    glfwMakeContextCurrent(window);

    //initializa GLAD, report if failed
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout<<"Failed to initialize GLAD. Aborting..."<<std::endl;
        return -1;
    }

    //sets the dimensions for rendering for the GL (finally smth about rendering)
    // first two values (x,y) are from -1 to 1 and are mapped to the actual screen coordinates
    glViewport(0, 0, 800, 600);

    //call the framebuffer_size_callback function everytime the "window" window's size is changed
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);



    //so called render loop
    //glfwWindowShouldClose just checks whether glfw has been instructed to close the window
    while (!glfwWindowShouldClose(window)) {

        processInput(window); //input

        //some rendering commands here

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f); //clear the whole screan with specified color (rgb,alpha)
        glClear(GL_COLOR_BUFFER_BIT); //not sure yet about this one

        glfwSwapBuffers(window); //swap the color buffer (2D array that contains color values for each pixel in GLFW window)

        glfwPollEvents(); //checks if any events are triggered (keyboard input, mouse movement, etc.), updates window state and calls the corresponding functions (which we can register via callback methods)

    }


    glfwTerminate();
    return 0;
}


