#include "ui.h"
#include "../data/local/LocalEngine.h"
#include "../data/BatchRunner.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "implot.h"
#include "portable-file-dialogs.h"
#include <GLFW/glfw3.h> 
#include <iostream>
#include <thread>
#include <numeric>
#include <filesystem>
#include <functional>

DataSource currentSource;
std::unique_ptr<SimulationEngine> engine;
std::unique_ptr<SimulationEngine> createEngine(int w = 50, int h = 50) {

    switch (currentSource) {
        case DataSource::LOCAL:
            return std::make_unique<LocalEngine>(w, h);
        default:
            std::cerr << "Unknown data source" << std::endl;
            return nullptr;
    }

}

void startGui(DataSource source) {
    currentSource = source;

    engine = createEngine(50, 50);

    launchGui();
}

bool is_playing = false;

// Main Writer: Kristian/Gecenio/Berke
// Reviewer: 
// Contributers: 
void launchGui() {
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
    double physics_tick_rate = 0.05; // Run 1 physics step every 0.5 seconds

    // The main loop
    while(!glfwWindowShouldClose(window)) { 
        glfwPollEvents();

        double current_time = glfwGetTime();
        
        SimulationState state = *engine->getState();
        
        if (is_playing && (current_time - last_physics_tick >= physics_tick_rate)) {
            // Check whether you are at the end of the computed frames and it is a necessary save
            if (state.current_step >= state.temperature_history.size() - 1 && state.grid_history.empty()) {
                is_playing = false;
            }
            else {
                engine->stepFoward();
            }

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
            ImGuiID dock_id_right_bottom = ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Down, 0.65f, nullptr, &dock_id_right);

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
            ImPlot::SetupAxesLimits(0, state.width, state.height, 0, ImGuiCond_Always);

            // Change the colors 
            // ImPlotColormap_Jet goes from Blue (Cold) to Red (Hot)
            // ImPlotColormap_Hot goes from Black (Cold) to White/Yellow (Hot)
            ImPlot::PushColormap(ImPlotColormap_Jet); 

            // Draw the heatmap
            ImPlot::PlotHeatmap("##HeatData", 
                                state.temperatures.data(), 
                                state.height, state.width,
                                20.0, 100.0,
                                nullptr,       // Custom label format (nullptr hides it)
                                ImPlotPoint(0, state.height), ImPlotPoint(state.width, 0));

            ImPlot::PopColormap();

            ImPlot::EndPlot();
        }        
        ImGui::End();

        // --- Simulation controls ---
        ImGui::Begin("Simulation Controls");

        ImGui::SeparatorText("Control Simulation");

        if (is_playing) {
            if (ImGui::Button("Pause Simulation")) {
                is_playing = false;
            }
        }      
        else {
            if (ImGui::Button("Play Simulation")) {
                is_playing = true;
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
        float inputWidth = (0.2f * windowWidth);
        
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
            // Get width
            ImGui::AlignTextToFramePadding();
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

            ImGui::SameLine();
            if (ImGui::Button("Confirm")) {
                // Kill the compute thread.
                engine->thread->terminate();

                // Create and display the new sim
                engine = std::move(createEngine(w, h));

                createNew = false;
            }
        }

        static bool save = false;
        static char filenameBuffer[256] = "sim_01";
        std::string folder = "../saves/";
        std::string path = folder + std::string(filenameBuffer) + ".dat";
        static const char* saveTypes[] = {"Necessary", "Complete"};
        static int selected = 0;

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
            ImGui::AlignTextToFramePadding();
            ImGui::Text("FileName:");
            ImGui::SameLine();
            ImGui::InputText("##File Name", filenameBuffer, sizeof(filenameBuffer));

            ImGui::PopItemWidth();

            SaveType saveType = selected == 0 ? SaveType::NECESSARY : SaveType::COMPLETE;

            ImGui::SameLine();
            if (ImGui::Button("Confirm")) {
                if (saveSimulation(state, path, saveType)) {
                    std::cout << "Saved to: " << path << std::endl;
                }
                else {
                    std::cerr << "Failed to save to: " << path << std::endl;
                }

                save = false;
            }
        }

        if (ImGui::Button("Load Simulation")) {
            auto f = pfd::open_file("Choose a save", "", {"Data FIles (.dat)", "*.dat", "All Files", "*"});
            
            if (!f.result().empty()) {
                std::string selectedPath = f.result()[0];

                switch (currentSource) {
                    case DataSource::LOCAL:
                        engine->thread->terminate();
                        auto loadedEngine = loadLocalSimulation(selectedPath);
                        if (loadedEngine) {
                            engine = std::move(loadedEngine);
                            std::cout << "Loaded new simulation from: " << selectedPath << std::endl;
                        }
                }
            }
        }
        
        static bool batch = false;

        if (ImGui::Button("Batch Simulations")) {
            if (!batch) {
                batch = true;
            }
            else {
                batch = false;
            }
        }
        if (batch) {
            static int batchW = 3; // Default values
            static int batchH = 2;
            static int NumberOfSims = 1;
            static int batchSelected = 0;

            ImGui::SeparatorText("Batch Settings");

            // Width and height Input
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Width:");
            ImGui::SameLine();
            ImGui::PushItemWidth(inputWidth);
            ImGui::InputInt("##Width", &batchW);

            ImGui::SameLine();
            ImGui::Text("Height:");
            ImGui::SameLine();
            ImGui::InputInt("##Height", &batchH);
            ImGui::PopItemWidth();

            // Simulation Input
            ImGui::AlignTextToFramePadding();
            ImGui::Text("# of Simulations:");
            ImGui::SameLine();
            ImGui::PushItemWidth(inputWidth);
            ImGui::InputInt("##NumberOfSims", &NumberOfSims);

            ImGui::SeparatorText("Batch Save File");

            // Dropdown menu for the save type
            ImGui::PushItemWidth(0.3f * windowWidth);
            if (ImGui::BeginCombo("##Save Type", saveTypes[batchSelected])) {
                for (int i = 0; i < IM_ARRAYSIZE(saveTypes); i++) {
                    bool is_selected = (batchSelected == i);
                    if (ImGui::Selectable(saveTypes[i], is_selected)) {
                        batchSelected = i;
                    }

                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::AlignTextToFramePadding();
            ImGui::Text("FileName:");
            ImGui::SameLine();
            ImGui::InputText("##File Name", filenameBuffer, sizeof(filenameBuffer));
            ImGui::PopItemWidth();

            SaveType saveType = selected == 0 ? SaveType::NECESSARY : SaveType::COMPLETE;

            // Estimated File Size
            int cells = batchW * batchH;
            size_t necessary = 24 + (8 * cells);
            size_t complete = 24 + (80 * cells);

            if (saveType == SaveType::NECESSARY) {
                ImGui::Text("Size per frame: %zu Bytes", necessary);
            }
            else {
                ImGui::Text("Size per frame: %zu Bytes", complete);
            }

            // Run Sims 
            if (ImGui::Button("Run Batch Simulations")) {
                std::string filename(filenameBuffer);
                std::thread batchThread = runSimulations(batchW, batchH, NumberOfSims, filename, saveType);
                batchThread.detach();
            }
        }

        ImGui::End();

        // --- Stats window ---
        ImGui::Begin("Stats");
        ImGui::SeparatorText("Temperature Data");

        double hotSpot = state.max_temp_history.back();
        double coldSpot = state.min_temp_history.back();
        double estMiddle = (hotSpot + coldSpot) / 2;

        // Live real data
        ImGui::Text("Hot Spot: %.2f C", hotSpot);
        ImGui::Text("Middle): %.2f C", estMiddle);
        ImGui::Text("Bottom Right: %.2f C", coldSpot);

        // Graph for temperature
        // Get available width and height
        float width = ImGui::GetContentRegionAvail().x;
        float height = ImGui::GetContentRegionAvail().y;

        // Create graph
        if (ImPlot::BeginPlot("Temperature Convergence", ImVec2(-1.0f, height * 0.5f))) {            
            
            // Setup Axis Labels
            ImPlot::SetupAxes("Time Step", "Temperature (°C)");
            
            // Make the X-Axis automatically scroll forward as time goes on
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, (state.current_step > 5 ? state.current_step + 1 : 5), ImGuiCond_Always);
            
            // Lock the Y-Axis between 15C and 105C so the graph doesn't jump around
            ImPlot::SetupAxisLimits(ImAxis_Y1, 15.0, 105.0, ImGuiCond_Once);

            // Plot real vectors
            // ImPlot takes the raw memory pointer (.data()) and the length of the array (.size())
            ImPlot::PlotLine("Max Temp (Hot Spot)", state.time_history.data(), state.max_temp_history.data(), state.time_history.size());
            ImPlot::PlotLine("Min Temp (Cold Spot)", state.time_history.data(), state.min_temp_history.data(), state.time_history.size());

            // Time step marker
            if (!state.time_history.empty() && state.current_step < state.time_history.size()) {
                // Get the current time, max and min
                double cur_time = state.time_history[state.current_step];
                double cur_max = state.max_temp_history[state.current_step];
                double cur_min = state.min_temp_history[state.current_step];

                ImPlotSpec specLine;
                specLine.LineColor = ImVec4(0.7f, 0.7f, 0.7f, 0.6f); // Grey
                specLine.LineWeight = 1.5f;
                ImPlot::PlotInfLines("##CurrentTime", &cur_time, 1, specLine);

                double x_dots[2] = {cur_time, cur_time};
                double y_dots[2] = {cur_max, cur_min};

                ImPlotSpec specDots;
                specDots.Marker = ImPlotMarker_Circle; // Make it a circle
                specDots.MarkerSize = 4.0f;
                specDots.MarkerFillColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White
                specDots.MarkerLineColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White
                ImPlot::PlotScatter("##CurrentTimeDots", x_dots, y_dots, 2, specDots);
            }

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
        double total_energy = std::accumulate(state.temperatures.begin(), state.temperatures.end(), 0.0);
        ImGui::Text("Total System Energy: %.2f J", total_energy);

        ImGui::End();

        // --- Timeline ---
        ImGui::Begin("Timeline");

        /// Get current limits of the simulation
        int currentStep = state.current_step;
        int maxStep = state.temperature_history.empty() ? 0 : state.temperature_history.size() - 1;

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::GetContentRegionAvail().y - 20) * 0.5f);

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderInt("##timeline", &currentStep, 0, maxStep, "Frame %d")) {
            // Pause sim
            is_playing = false;

            // Tell the engine to update the simulation
            engine->seekTo(currentStep);
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
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}
