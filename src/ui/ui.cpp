#include "ui.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> 
#include <iostream>

void startGui() {
    // First, we need to initialize GLFW which is our window manager.
    // We'll use GLFW to render a window, and imGui to draw to it.

    glfwInit(); 

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Heat Transfer Simulation", NULL, NULL);

    glfwMakeContextCurrent(window); // Has the effect of making it the "main" window imGui will draw to.

    // Now, we need to initialize imGui.
    ImGui::CreateContext();
    
    // Setup for docking and viewports which allows for windows to be easily resized with the application
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;


    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // The main loop
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Dockspace background setup
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | 
                                        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | 
                                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | 
                                        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::Begin("MainWorkspace", nullptr, window_flags);
        ImGui::PopStyleVar();
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        
        // Dockbuilder Initial layout
        static bool first_time = true;
        if (first_time) {
            first_time =false;

            ImGui::DockBuilderRemoveNode(dockspace_id); 
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

            // Split the dockspace into our designated zones
            ImGuiID dock_main_id = dockspace_id;
            ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.10f, nullptr, &dock_main_id);
            ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.33f, nullptr, &dock_main_id);
            ImGuiID dock_id_right_bottom = ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Down, 0.50f, nullptr, &dock_id_right);

            // Assign windows to those zones based on their title
            ImGui::DockBuilderDockWindow("Simulation", dock_main_id);
            ImGui::DockBuilderDockWindow("Stats", dock_id_right);
            ImGui::DockBuilderDockWindow("Simulation Controls", dock_id_right_bottom);
            ImGui::DockBuilderDockWindow("Timeline", dock_id_bottom);

            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::End();

        // Gui elements go down below
        
        // --- Simulation window ---
        ImGui::Begin("Simulation");
        ImGui::Text("Simulation goes here");
        ImGui::End();

        // --- Stats window ---
        ImGui::Begin("Stats");
        ImGui::Text("Statistics: ");
        ImGui::End();

        // --- Simulation controls ---
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
        ImGui::Begin("Timeline");

        static float currentTime = 0.0f;
        float totalTime = 10.0f;

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - 20) * 0.5f);

        ImGui::SetNextItemWidth(-1.0f);
        // Hide the label by using "##"
        if (ImGui::SliderFloat("##timeline", &currentTime, 0.0f, totalTime, "%.2f")) {
            std::cout << "Scrolling timeline...\n";
        }

        ImGui::End();


        // End of gui elements

        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Last bit of docking
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }
}