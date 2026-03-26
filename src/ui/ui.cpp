#include "ui.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> 

void startGui() {
    // First, we need to initialize GLFW which is our window manager.
    // We'll use GLFW to render a window, and imGui to draw to it.

    glfwInit(); 

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Heat Transfer Simulat", NULL, NULL);

    glfwMakeContextCurrent(window); // Has the effect of making it the "main" window imGui will draw to.

    // Now, we need to initialize imGui.
    ImGui::CreateContext();
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // The main loop
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();
        // Gui elements go down below

        // Test window
        ImGui::Begin("Hello, world!");
        ImGui::Text("Hello once again!");

        if (ImGui::Button("Exit")) {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }

        // End of previous window
        ImGui::End();
        
        // --- Simulation window ---
        // Set size of 1280,720 and place it at the top left corner
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(1280, 900));

        // Create the window
        ImGui::Begin("Simulation");
        ImGui::Text("Simulation goes here");
        ImGui::End();

        // --- Stats window ---
        // Set size of 640,360 and place it right next to the simulation window
        ImGui::SetNextWindowPos(ImVec2(1280, 0));
        ImGui::SetNextWindowSize(ImVec2(640, 450));

        // Create the window
        ImGui::Begin("Stats");
        ImGui::Text("Stastics: ");
        ImGui::End();

        // --- Simulation controls ---
        // Set size of 640,360 and place it right next to the simulation window and below the stats window
        ImGui::SetNextWindowPos(ImVec2(1280, 450));
        ImGui::SetNextWindowSize(ImVec2(640, 450));

        // Create the window
        ImGui::Begin("Simulation Controls");
        if (ImGui::Button("Start Simulation")) {
            // TODO: Actually starts simulation
        }
        if (ImGui::Button("Change Angle")) {
            // TODO: Actually changes 2d to 3d
        }
        if (ImGui::Button("Heat Map")) {
            // TODO: Actually shows heat map
        }
        ImGui::End();

        // --- Timeline ---
        ImGui::SetNextWindowPos(ImVec2(0, 900));
        ImGui::SetNextWindowSize(ImVec2(1920, 180));

        // Create the window
        ImGui::Begin("Timeline");
        ImGui::Text("Placeholder");
        ImGui::End();


        // End of gui elements

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