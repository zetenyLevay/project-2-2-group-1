#include "runSims.h"
#include "ui.h"
#include "../data/BatchRunner.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> 

void launchSims() {
    glfwInit(); 

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(400, 400, "Run New Simulations", NULL, NULL);
    glfwMakeContextCurrent(window); // Has the effect of making it the "main" window imGui will draw to.

    // Now, we need to initialize imGui and implot
    ImGui::CreateContext();
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

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

        ImGui::Begin("##Run New Simulations", nullptr, window_flags);
       
        // -- Simulation Settings --
        ImGui::SeparatorText("Simulation Settings");

        // Width and Height Input 
        static int w = 3; // Default values
        static int h = 2;
        float windowWidth = ImGui::GetContentRegionAvail().x;
        float inputWidth = (0.2f * windowWidth);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Width:");
        ImGui::SameLine();
        ImGui::PushItemWidth(inputWidth);
        ImGui::InputInt("##Width", &w);

        ImGui::SameLine();
        ImGui::Text("Height:");
        ImGui::SameLine();
        ImGui::InputInt("##Height", &h);

        ImGui::PopItemWidth();

        // Number of Simulations 
        static int NumberOfSims = 1;

        ImGui::AlignTextToFramePadding();
        ImGui::Text("# of Simulations:");
        ImGui::SameLine();
        ImGui::PushItemWidth(inputWidth);
        ImGui::InputInt("##NumberOfSims", &NumberOfSims);
        
        // -- Save type --
        ImGui::SeparatorText("Save File");

        // Choosing the save type
        static char filenameBuffer[256] = "sim";
        std::string folder = "../saves/";
        std::string path = folder + std::string(filenameBuffer) + ".dat";
        static const char* saveTypes[] = {"Necessary", "Complete"};
        static int selected = 0;

        ImGui::PushItemWidth(0.3f * windowWidth);

        // Dropdown menu for the save type
        if (ImGui::BeginCombo("##Save Type", saveTypes[selected])) {
            for (int i = 0; i < IM_ARRAYSIZE(saveTypes); i++) {
                bool is_selected = (selected == i);
                if (ImGui::Selectable(saveTypes[i], is_selected)) {
                    selected = i;
                }

                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        
        ImGui::Text("FileName:");
        ImGui::SameLine();
        ImGui::InputText("##File Name", filenameBuffer, sizeof(filenameBuffer));

        ImGui::PopItemWidth();

        SaveType saveType = selected == 0 ? SaveType::NECESSARY : SaveType::COMPLETE;

        // Estimated File Size
        int cells = w * h;

        size_t necessary = 24 + (8 * cells);
        size_t complete = 24 + (80 * cells);

        if (saveType == SaveType::NECESSARY) {
            ImGui::Text("Size per frame: %zu Bytes", necessary);
        }
        else {
            ImGui::Text("Size per frame: %zu Bytes", complete);
        }

        // -- Run Sims --
        ImGui::SeparatorText("Run Simulation(s)");
        if (ImGui::Button("Confirm")) {
            std::string filename(filenameBuffer);
            runSimulations(w, h, NumberOfSims, filename, saveType);
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
}