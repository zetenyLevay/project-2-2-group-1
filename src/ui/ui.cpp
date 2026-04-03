#include "ui.h"
#include "SimulationEngine.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "implot.h"
#include <GLFW/glfw3.h> 
#include <iostream>
#include <thread>
#include <numeric>

void startGui(std::unique_ptr<SimulationEngine>& engine) {
    // First, we need to initialize GLFW which is our window manager.
    // We'll use GLFW to render a window, and imGui to draw to it.

    glfwInit(); 

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Heat Transfer Simulation", NULL, NULL);
    glfwMakeContextCurrent(window); // Has the effect of making it the "main" window imGui will draw to.

    // Now, we need to initialize imGui and implot
    ImGui::CreateContext();
    ImPlot::CreateContext();
    
    // Setup for docking and viewports which allows for windows to be easily resized with the application
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    double last_physics_tick = glfwGetTime();
    double physics_tick_rate = 0.5; // Run 1 physics step every 0.5 seconds

    // The main loop
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double current_time = glfwGetTime();        

        if (engine->is_playing && (current_time - last_physics_tick >= physics_tick_rate)) {            \
            engine->stepFoward();
            
            // Reset the timer for the next tick
            last_physics_tick = current_time;
        }

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
            ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.20f, nullptr, &dock_main_id);
            ImGuiID dock_id_right_bottom = ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Down, 0.70f, nullptr, &dock_id_right);

            // Assign windows to those zones based on their title
            ImGui::DockBuilderDockWindow("Simulation", dock_main_id);
            ImGui::DockBuilderDockWindow("Simulation Controls", dock_id_right);
            ImGui::DockBuilderDockWindow("Stats", dock_id_right_bottom);
            ImGui::DockBuilderDockWindow("Timeline", dock_id_bottom);

            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::End();

        // Gui elements go down below
        
        // --- Simulation window ---
        ImGui::Begin("Simulation");

        // We use -1.0f to make the plot fill the entire available window space
        if (ImPlot::BeginPlot("##HeatmapCanvas", ImVec2(-1.0f, -1.0f), ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
            
            // Hide the X and Y axes so it looks like a  2D canvas and not a graph
            ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
            
            // Force the plot to match the exact dimensions of the grid
            ImPlot::SetupAxesLimits(0, engine->width, engine->height, 0, ImGuiCond_Always);

            // Change the colors 
            // ImPlotColormap_Jet goes from Blue (Cold) to Red (Hot)
            // ImPlotColormap_Hot goes from Black (Cold) to White/Yellow (Hot)
            ImPlot::PushColormap(ImPlotColormap_Jet); 

            // Draw the heatmap
            ImPlot::PlotHeatmap("##HeatData", 
                                engine->temperatures.data(), 
                                engine->height, engine->width,
                                20.0, 100.0,
                                nullptr,       // Custom label format (nullptr hides it)
                                ImPlotPoint(0, engine->height), ImPlotPoint(engine->width, 0));

            ImPlot::PopColormap();

            ImPlot::EndPlot();
        }        
        ImGui::End();

        // --- Simulation controls ---
        ImGui::Begin("Simulation Controls");
        ImGui::SeparatorText("Control Simulation");

        if (engine->is_playing) {
            if (ImGui::Button("Pause Simulation")) {
                engine->is_playing = false;
            }
        }      
        else {
            if (ImGui::Button("Play Simulation")) {
                engine->is_playing = true;
            }
        }
        
        if (ImGui::Button("Step Forward (One Frame)")) {
            engine->stepFoward();
        }

        if (ImGui::Button("Step Back (One Frame)")) {
            engine->stepBack();
        }
        
        ImGui::SeparatorText("Change Simulation");

        static bool createNew = false;
        static int w = 3; // Default values
        static int h = 2;
        float windowWidth = ImGui::GetContentRegionAvail().x;
        
        // Click to open/close create dropdown
        if (ImGui::Button("Create New Simulation")) {
            if (!createNew) {
                createNew = true;
            }
            else {
                createNew = false;
            }
        }
        if (createNew) {
            float inputWidth = (0.2f * windowWidth);

            // Get width
            ImGui::Text("Width:");
            ImGui::SameLine();
            ImGui::PushItemWidth(inputWidth);
            ImGui::InputInt("##Width", &w);

            // Get height
            ImGui::SameLine();
            ImGui::Text("Height:");
            ImGui::SameLine();
            ImGui::InputInt("##Height", &h);

            ImGui::PopItemWidth();

            if (ImGui::Button("Confirm")) {
                // Create and display the new sim
                engine = std::make_unique<SimulationEngine>(w, h);

                createNew = false;
            }
        }

        static bool save = false;
        static char filenameBuffer[256] = "sim_01.dat";
        std::string folder = "../saves/";
        std::string path = folder + std::string(filenameBuffer);

        // Click to open/close save dropdown
        if (ImGui::Button("Save Simulation")) {
            if (!save) {
                save = true;
            }
            else {
                save = false;
            }
        }
        if (save) {
            ImGui::Text("FileName:");
            ImGui::SameLine();
            ImGui::PushItemWidth(0.3f * windowWidth);
            ImGui::InputText("##File Name", filenameBuffer, sizeof(filenameBuffer));

            ImGui::PopItemWidth();

            ImGui::SameLine();
            if (ImGui::Button("Confirm")) {
                if (engine->saveSimulation(path)) {
                    std::cout << "Saved to: " << path << std::endl;
                }
                else {
                    std::cerr << "Failed to save to: " << path << std::endl;
                }

                save = false;
            }
        }

        if (ImGui::Button("Load Simulation")) {
            auto loadedEngine = SimulationEngine::loadSimulation(path);
            if (loadedEngine) {
                engine = std::move(loadedEngine);
                std::cout << "Loaded new simulation from: " << path << std::endl;
            }
        }

        ImGui::End();

        // --- Stats window ---
        ImGui::Begin("Stats");
        ImGui::SeparatorText("Temperature Data");

        // Live real data
        ImGui::Text("Hot Spot (0,0): %.2f C", engine->temperatures[engine->getIndex(0,0)]);
        ImGui::Text("Middle (1,0): %.2f C", engine->temperatures[engine->getIndex(1,0)]);
        ImGui::Text("Bottom Right (2,1): %.2f C", engine->temperatures[engine->getIndex(2,1)]);

        // Graph for temperature
        // Example data
        static float x_time[] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
        static float y_temp[] = { 0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.2f };

        // Get available width and height
        float width = ImGui::GetContentRegionAvail().x;
        float height = ImGui::GetContentRegionAvail().y;

        // Create graph
        if (ImPlot::BeginPlot("Temperature Convergence", ImVec2(-1.0f, height * 0.5f))) {            
            
            // Setup Axis Labels
            ImPlot::SetupAxes("Time Step", "Temperature (°C)");
            
            // Make the X-Axis automatically scroll forward as time goes on
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, (engine->current_step > 5 ? engine->current_step + 1 : 5), ImGuiCond_Always);
            
            // Lock the Y-Axis between 15C and 105C so the graph doesn't jump around
            ImPlot::SetupAxisLimits(ImAxis_Y1, 15.0, 105.0, ImGuiCond_Once);

            // Plot real vectors
            // ImPlot takes the raw memory pointer (.data()) and the length of the array (.size())
            ImPlot::PlotLine("Max Temp (Hot Spot)", engine->time_history.data(), engine->max_temp_history.data(), engine->time_history.size());
            ImPlot::PlotLine("Min Temp (Cold Spot)", engine->time_history.data(), engine->min_temp_history.data(), engine->time_history.size());

            ImPlot::EndPlot();
        }

        // Space between sections
        ImGui::Spacing();

        // Simulation Info
        ImGui::SeparatorText("Engine & Hardware Information");
        
        // 1. Hardware Detection
        unsigned int cpu_threads = std::thread::hardware_concurrency();
        ImGui::Text("Host Hardware: %d CPU Threads", cpu_threads);
        ImGui::Text("CUDA GPU: None (CPU Prototype Mode)");

        // 2. LBM Grid Stats
        ImGui::Text("Grid Size: %d x %d", engine->width, engine->height);
        ImGui::Text("Memory Nodes: %d cells", engine->cells);

        // 3. Thermodynamic Conservation
        // Sums up every temperature in the grid to prove no heat is lost
        double total_energy = std::accumulate(engine->temperatures.begin(), engine->temperatures.end(), 0.0);
        ImGui::Text("Total System Energy: %.2f J", total_energy);

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