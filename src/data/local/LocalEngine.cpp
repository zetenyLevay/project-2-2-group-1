#include "LocalEngine.h"
#include <numeric>
#include <fstream>
#include <iostream>
#include <filesystem>

// Initialized on main thread
LocalEngine::LocalEngine(int width, int height) : SimulationEngine(width, height) {
    auto initialState = std::make_shared<SimulationState>();

    initialState->width = width;
    initialState->height = height;

    auto cells = initialState->cells = width * height;

    initialState->grid = initialState->cells;

    initialState->current_step = 0;
    initialState->heat_spread = 1.0;

    initialState->temperatures.resize(cells, 20.0); // room temp assumption
    initialState->temperatures[getIndex(0,0)] = MAX_TEMP; // Set heat source 

    // Initialize Grid 
    for (int i = 0; i < cells; i++) {
        for (int d = 0; d < 9; ++d) {
            initialState->grid.g[d][i] = weights[d] * initialState->temperatures[i];
        }
    }

    initialState->time_history.push_back(initialState->current_step);
    initialState->max_temp_history.push_back(MAX_TEMP);
    initialState->min_temp_history.push_back(ROOM_TEMP);
    initialState->temperature_history.push_back(initialState->temperatures);
    initialState->grid_history.push_back(initialState->grid);

    // Launch the compute thread.
    this->thread = std::make_unique<ReusableThread>(initialState);
}

// Mutates: grid, temperatures, current_step, time_history, max_temp_history, min_temp_history, temperature_history, grid_history
void LocalEngine::stepFoward() {
    thread->submitTask([this](SimulationState& state) {
        // If already calculated just set the grid and temperatures again 
        if (state.current_step < state.temperature_history.size() - 1) {
            state.current_step++;
            state.temperatures = state.temperature_history[state.current_step];

            // Only pull from grid_history if it exists (Necessary loads dont have grid history)
            if (!state.grid_history.empty() && state.current_step < state.grid_history.size()) {
                state.grid = state.grid_history[state.current_step];
            }
            return;
        }

        if (state.grid_history.empty()) {
            std::cout << "You have loaded a 'Necessary' save, you cannot compute new frames" << std::endl;
            return;
        }

        Grid gridTemp(state.cells);
        this->Collision(state.heat_spread, gridTemp, state.grid);

        this->Stream(gridTemp, state.grid);

        double current_max = ROOM_TEMP;
        double current_min = MAX_TEMP;

        for (int i = 0; i < cells; i++) {
            double temp = 0.0;

            for (int d = 0; d < 9; ++d) {
                temp += state.grid.g[d][i];
            }
            state.temperatures[i] = temp;

            // Find Max and Min for the graph
            if (state.temperatures[i] > current_max) current_max = state.temperatures[i];
            if (state.temperatures[i] < current_min) current_min = state.temperatures[i];
        }

        state.current_step++;
        state.time_history.push_back(state.current_step);
        state.max_temp_history.push_back(current_max);
        state.min_temp_history.push_back(current_min);
        state.temperature_history.push_back(state.temperatures);

        // Save grid state
        state.grid_history.push_back(state.grid);
    });
}

// Mutates: current_step, time_history, max_temp_history, min_temp_history, temperature_history, grid_history, temperatures, grid
void LocalEngine::stepBack() {
    thread->submitTask([this](SimulationState& state) {
        // Prevent going back beyond initial state
        if (state.current_step <= 0) return;
    
        // Decrement the current step
        state.current_step--;

        state.temperatures = state.temperature_history[state.current_step];

        // Only pull from grid_history if it exists (Necessary loads dont have grid history)
        if (!state.grid_history.empty() && state.current_step < state.grid_history.size()) {
            state.grid = state.grid_history[state.current_step];
        }
        
        /* 
        // Erase most recent state
        // WARNING: I THINK THIS WILL CAUSE ISSUES WHEN ADD THE ABILITY TO LOAD IN SIMS
        state.time_history.pop_back();
        state.max_temp_history.pop_back();
        state.min_temp_history.pop_back();
        state.temperature_history.pop_back();
        state.grid_history.pop_back();

        // Restore previous state
        state.temperatures = state.temperature_history.back();
        state.grid = state.grid_history.back();
        */
    });
}

double LocalEngine::getTotalEnergy() const {
    // Can't really make this run on a seperate thread without changing the function signature.
    auto state = thread->getState();
    return std::accumulate(state->temperatures.begin(), state->temperatures.end(), 0.0);
}


// Physics Functions (LBM)
// I will assume these functions are already running on the compute thread.
void LocalEngine::Collision(double heat_spread, Grid& gridTemp, const Grid grid) {
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

void LocalEngine::Stream(const Grid gridTemp, Grid &grid) {
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

std::unique_ptr<LocalEngine> loadLocalSimulation(const std::string& filepath) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return nullptr;
    }

    // Savetype
    SaveType saveType;
    in.read(reinterpret_cast<char*>(&saveType), sizeof(saveType));

    // Width and Height
    int w, h;
    in.read(reinterpret_cast<char*>(&w), sizeof(w));
    in.read(reinterpret_cast<char*>(&h), sizeof(h));

    // Create Engine
    auto loadedEngine = std::make_unique<LocalEngine>(w, h);
    auto state = loadedEngine->getMutableState();

    // Get history length
    size_t history_count;
    in.read(reinterpret_cast<char*>(&history_count), sizeof(history_count));

    // Read basic history information
    state->time_history.resize(history_count);
    state->max_temp_history.resize(history_count);
    state->min_temp_history.resize(history_count);

    in.read(reinterpret_cast<char*>(state->time_history.data()), history_count * sizeof(double));
    in.read(reinterpret_cast<char*>(state->max_temp_history.data()), history_count * sizeof(double));
    in.read(reinterpret_cast<char*>(state->min_temp_history.data()), history_count * sizeof(double));

    // Get full temperature history
    state->temperature_history.resize(history_count, std::vector<double>(state->cells));
    for (size_t i = 0; i < history_count; ++i) {
        in.read(reinterpret_cast<char*>(state->temperature_history[i].data()), state->cells * sizeof(double));
    }

    // Clear the grid history because the engine constructer adds an initial state
    state->grid_history.clear();

    if (saveType == SaveType::COMPLETE) {
        state->grid_history.reserve(history_count);

        // Get full grid history
        for (size_t i = 0; i < history_count; ++i) {
            Grid tempGrid(state->cells);

        for (int d = 0; d < 9; ++d) {
            in.read(reinterpret_cast<char*>(tempGrid.g[d].data()), state->cells * sizeof(double));
        }
        state->grid_history.push_back(tempGrid);
    }
    }
    
    // Go to the last frame of the sim
    if (history_count > 0) {
        state->current_step = state->time_history.back();
        state->temperatures = state->temperature_history.back();

        if (saveType == SaveType::COMPLETE) {
            state->grid = state->grid_history.back();
        }
    }

    in.close();
    return loadedEngine;
}

bool saveSimulation(const SimulationState state, const std::string& filepath, const SaveType saveType) {
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
    out.write(reinterpret_cast<const char*>(&state.width), sizeof(state.width));
    out.write(reinterpret_cast<const char*>(&state.height), sizeof(state.height));

    // Write history length
    size_t history_count = state.time_history.size();
    out.write(reinterpret_cast<const char*>(&history_count), sizeof(history_count));

    // Write basic history information
    out.write(reinterpret_cast<const char*>(state.time_history.data()), history_count * sizeof(double));
    out.write(reinterpret_cast<const char*>(state.max_temp_history.data()), history_count * sizeof(double));
    out.write(reinterpret_cast<const char*>(state.min_temp_history.data()), history_count * sizeof(double));

    // Write temperature history
    for (size_t i = 0; i < history_count; ++i) {
        out.write(reinterpret_cast<const char*>(state.temperature_history[i].data()), state.cells * sizeof(double));
    }

    if (saveType == SaveType::COMPLETE) {
        // Write grid history
        for (size_t i = 0; i < history_count; ++i) {
            for (int d = 0; d < 9; ++d) {
                out.write(reinterpret_cast<const char*>(state.grid_history[i].g[d].data()), state.cells * sizeof(double));
            }
        }
    }

    out.close();
    return true;
}