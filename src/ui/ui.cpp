#include "ui.h"
#include "imgui.h"

void startGui() {
    // First, we need to initialize GLFW which is our window manager.
    // We'll use GLFW to render a window, and imGui to draw to it.

    glfwInit(); 

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(640, 480, "My Title", NULL, NULL);

    glfwMakeContextCurrent(window); // Has the effect of making it the "main" window imGui will draw to.

    // Now, we need to initialize imGui.
    ImGui::CreateContext();
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    // The main loop
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();
        // Gui elements go down below

        ImGui::Begin("Hello, world!");
        ImGui::Text("Hello once again!");

        if (ImGui::Button("Exit")) {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }

        // End of gui elements
        ImGui::End();

        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
}