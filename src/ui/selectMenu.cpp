#include "selectMenu.h"
#include "ui.h"
#include "runSims.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> 

void launchSelectMenu() {
    glfwInit(); 

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(200, 100, "Select Menu", NULL, NULL);
    glfwMakeContextCurrent(window); // Has the effect of making it the "main" window imGui will draw to.

    // Now, we need to initialize imGui and implot
    ImGui::CreateContext();
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    bool launchUI = false;
    bool launchSim = false;
    DataSource source = DataSource::LOCAL;

    // Main loop
    while(!glfwWindowShouldClose(window)) {
        // --- SETUP ---
        glfwPollEvents();

        ImGui_ImplGlfw_NewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

        // --- GUI ELEMENTS ---
        ImGui::Begin("##Select Menu", nullptr, window_flags);

        //ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("Select an option");

        if (ImGui::Button("View Simulations")) {
            launchUI = true;
            source = DataSource::LOCAL;
            break;
        }

        if (ImGui::Button("Create New Simulations")) {
            launchSim = true;
            break;
        }

        ImGui::End();

        // --- RENDERING ---
        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    if (launchUI) {
        startGui(source);
    }

    if (launchSim) {
        launchSims();
    }
}