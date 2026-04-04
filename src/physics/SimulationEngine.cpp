#include "SimulationEngine.h"
#include <numeric>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <string.h>

SimulationEngine::SimulationEngine(int w, int h) 
    : width(w), height(h), cells(w * h), grid(cells), gridTemp(cells)
{
    current_step = 0;
    is_playing = false;
    heat_spread = 1.0;

    temperatures.resize(cells, 20.0); // room temp assumption
    temperatures[getIndex(0,0)] = MAX_TEMP; // Set heat source 

    // Initialize Grid 
    for (int i = 0; i < cells; i++) {
        for (int d = 0; d < 9; ++d) {
            grid.g[d][i] = weights[d] * temperatures[i];
        }
    }

    // Record initial state
    time_history.push_back(current_step);
    max_temp_history.push_back(MAX_TEMP);
    min_temp_history.push_back(ROOM_TEMP);
    temperature_history.push_back(temperatures);
    grid_history.push_back(grid);
}

int SimulationEngine::getIndex(int x, int y) {
    int wrappedX = (x + width) % width;
    int wrappedY = (y + height) % height;
    return wrappedY * width + wrappedX;
}

void SimulationEngine::stepFoward() {
    // If already calculated just set the grid and temperatures again 
    if (current_step < temperature_history.size() - 1) {
        current_step++;
        temperatures = temperature_history[current_step];
        grid = grid_history[current_step];
        return;
    }

    Collision(heat_spread);
    Stream();

    double current_max = ROOM_TEMP;
    double current_min = MAX_TEMP; // Assuming max 100 is possible

    for (int i = 0; i < cells; i++) {
        double temp = 0.0;

        for (int d = 0; d < 9; ++d) {
            temp += grid.g[d][i];
        }
        temperatures[i] = temp;

        // Find Max and Min for the graph
        if (temperatures[i] > current_max) current_max = temperatures[i];
        if (temperatures[i] < current_min) current_min = temperatures[i];
    }

    current_step++;
    time_history.push_back(current_step);
    max_temp_history.push_back(current_max);
    min_temp_history.push_back(current_min);
    temperature_history.push_back(temperatures);

    // Save grid state
    grid_history.push_back(grid);
}

void SimulationEngine::stepBack() {
    // Prevent going back beyond initial state
    if (current_step <= 0) return;

    // Decrement the current step
    current_step--;

    temperatures = temperature_history[current_step];
    grid = grid_history[current_step];

    /*
    // Erase most recent state
    // WARNING: I THINK THIS WILL CAUSE ISSUES WHEN ADD THE ABILITY TO LOAD IN SIMS 
    time_history.pop_back();
    max_temp_history.pop_back();
    min_temp_history.pop_back();
    temperature_history.pop_back();
    grid_history.pop_back();

    // Restore previous state
    temperatures = temperature_history.back();
    grid = grid_history.back();
    */
}

// Physics Functions (LBM)
void SimulationEngine::Collision(double heat_spread) {
    for (int y = 0; y < height; y++){
        for(int x = 0; x < width; x++){
            int idx= getIndex(x,y);

            // Calculating temperature of every g inside a cell
            double temp = 0.0;
            for (int d = 0; d < 9; ++d) {
                temp += grid.g[d][idx];
            }

            // Calculating the equilibrium function for every g inside of a cell and applying the collision to a new grid
            for (int d = 0; d < 9; ++d) {
                gridTemp.g[d][idx] = grid.g[d][idx] - (1.0/heat_spread) * (grid.g[d][idx] - weights[d] * temp);
            }
        }
    }
}

void SimulationEngine::Stream() {
    for(int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Current cell 1D index
            int currentIndex = getIndex(x, y);

            // Streaming each direction
            // In SoA the main idea is to write
            // from the old grid current index
            // to the new grid neighbor index
            for (int d = 0; d < 9; ++d) {
                int nindex = getIndex(x + cx[d], y + cy[d]);
                grid.g[d][nindex] = gridTemp.g[d][currentIndex];
            }
        }
    }
}

double SimulationEngine::getTotalEnergy() const {
    return std::accumulate(temperatures.begin(), temperatures.end(), 0.0);
}

bool SimulationEngine::saveSimulation(const std::string& filepath, const char* saveType) {
    std::filesystem::path pathObj(filepath);
    std::filesystem::path dir = pathObj.parent_path();

    if (!dir.empty() && !std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
        std::cout << "Created missing saves folder" << std::endl;
    }

    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) return false;

    // Write the type of save
    out.write(reinterpret_cast<const char*>(&saveType), sizeof(saveType));

    // Write width and height
    out.write(reinterpret_cast<const char*>(&width), sizeof(width));
    out.write(reinterpret_cast<const char*>(&height), sizeof(height));

    // Write history length
    size_t history_count = time_history.size();
    out.write(reinterpret_cast<const char*>(&history_count), sizeof(history_count));

    // Write basic history information
    out.write(reinterpret_cast<const char*>(time_history.data()), history_count * sizeof(double));
    out.write(reinterpret_cast<const char*>(max_temp_history.data()), history_count * sizeof(double));
    out.write(reinterpret_cast<const char*>(min_temp_history.data()), history_count * sizeof(double));

    // Write temperature history
    for (size_t i = 0; i < history_count; ++i) {
        out.write(reinterpret_cast<const char*>(temperature_history[i].data()), cells * sizeof(double));
    }

    if (strcmp(saveType, "Complete") == 0) {
        // Write grid history
        for (size_t i = 0; i < history_count; ++i) {
            for (int d = 0; d < 9; ++d) {
                out.write(reinterpret_cast<const char*>(grid_history[i].g[d].data()), cells * sizeof(double));
            }
        }
    }

    out.close();
    return true;
}

std::unique_ptr<SimulationEngine> SimulationEngine::loadSimulation(const std::string& filepath) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return nullptr;
    }

    // Savetype
    char* saveType;
    in.read(reinterpret_cast<char*>(&saveType), sizeof(saveType));

    // Width and Height
    int w, h;
    in.read(reinterpret_cast<char*>(&w), sizeof(w));
    in.read(reinterpret_cast<char*>(&h), sizeof(h));

    // Create Engine
    auto loadedEngine = std::make_unique<SimulationEngine>(w, h);

    // Get history length
    size_t history_count;
    in.read(reinterpret_cast<char*>(&history_count), sizeof(history_count));

    // Read basic history information
    loadedEngine->time_history.resize(history_count);
    loadedEngine->max_temp_history.resize(history_count);
    loadedEngine->min_temp_history.resize(history_count);

    in.read(reinterpret_cast<char*>(loadedEngine->time_history.data()), history_count * sizeof(double));
    in.read(reinterpret_cast<char*>(loadedEngine->max_temp_history.data()), history_count * sizeof(double));
    in.read(reinterpret_cast<char*>(loadedEngine->min_temp_history.data()), history_count * sizeof(double));

    // Get full temperature history
    loadedEngine->temperature_history.resize(history_count, std::vector<double>(loadedEngine->cells));
    for (size_t i = 0; i < history_count; ++i) {
        in.read(reinterpret_cast<char*>(loadedEngine->temperature_history[i].data()), loadedEngine->cells * sizeof(double));
    }

    // Clear the grid history because the engine constructer adds an initial state
    loadedEngine->grid_history.clear();

    if (strcmp(saveType, "Complete") == 0) {
        loadedEngine->grid_history.reserve(history_count);

        // Get full grid history
        for (size_t i = 0; i < history_count; ++i) {
            Grid tempGrid(loadedEngine->cells);

        for (int d = 0; d < 9; ++d) {
            in.read(reinterpret_cast<char*>(tempGrid.g[d].data()), loadedEngine->cells * sizeof(double));
        }
        loadedEngine->grid_history.push_back(tempGrid);
    }
    }
    
    // Go to the last frame of the sim
    if (history_count > 0) {
        loadedEngine->current_step = loadedEngine->time_history.back();
        loadedEngine->temperatures = loadedEngine->temperature_history.back();

        if (strcmp(saveType, "Complete") == 0) {
            loadedEngine->grid = loadedEngine->grid_history.back();
        }
    }

    in.close();
    return loadedEngine;
}