#pragma once
#include "main.h"

struct SimulationState;
class ReusableThread;


#include <vector>
#include <string>
#include <memory>
#include <filesystem>
#include <iostream>
#include <fstream>

struct SimulationState {
    int width, height, cells;
    Grid grid;
    std::vector<double> temperatures;
    double heatSource;
    bool isConstantHeatSource;
    
    // Make grid default size 0.
    SimulationState(): grid(0) {}

    // State Checks
    int current_step;
    double heat_spread;
    double tauT; //relaxation time temeprature
    double viscosity;
    double tauF; //relaxation time fluid
    double TempAvg;
};

struct SimulationHistory {
    // Information for the stats
    std::vector<double> time_history;
    std::vector<double> max_temp_history;
    std::vector<double> min_temp_history;
    std::vector<std::vector<double>> temperature_history;
};

class SimulationEngine {
public:
    int width, height, cells;

    std::unique_ptr<ReusableThread> thread;

    const SimulationState* getState();

    SimulationEngine(int w, int h);

    int getIndex(int x, int y);

    virtual ~SimulationEngine();

    // Step foward one frame
    virtual void stepFoward() = 0;

    virtual void stepBack() = 0;

    virtual void seekTo(int step) = 0;

    bool getAutoPlayStatus();
    void setAutoPlayStatus(bool status);

    const SimulationHistory* getReadOnlyHistory();

    protected:
        std::unique_ptr<SimulationHistory> history;

    private:
        bool autoPlay = false;    
};

enum DataSource {
    LOCAL,
    LOCAL_CUDA
};

// Main Writer: Kristian
// Reviewer: 
// Contributers: Berke
// Do not move to a cpp file, because of the template this function must be in this header file.
template <std::derived_from<SimulationEngine> T>
std::unique_ptr<T> loadSimulation(const std::string& filepath) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return nullptr;
    }

    // Width and Height
    int w, h;
    in.read(reinterpret_cast<char*>(&w), sizeof(w));
    in.read(reinterpret_cast<char*>(&h), sizeof(h));

    bool constantHeat;
    in.read(reinterpret_cast<char*>(&constantHeat), sizeof(constantHeat));

    std::unique_ptr<SimulationState> state = std::make_unique<SimulationState>();
    std::unique_ptr<SimulationHistory> history = std::make_unique<SimulationHistory>();

    // Get history length
    size_t history_count;
    in.read(reinterpret_cast<char*>(&history_count), sizeof(history_count));

    // Read basic history information
    history->time_history.resize(history_count);
    history->max_temp_history.resize(history_count);
    history->min_temp_history.resize(history_count);

    in.read(reinterpret_cast<char*>(history->time_history.data()), history_count * sizeof(double));
    in.read(reinterpret_cast<char*>(history->max_temp_history.data()), history_count * sizeof(double));
    in.read(reinterpret_cast<char*>(history->min_temp_history.data()), history_count * sizeof(double));

    // Get full temperature history
    history->temperature_history.resize(history_count, std::vector<double>(state->cells));
    for (size_t i = 0; i < history_count; ++i) {
        in.read(reinterpret_cast<char*>(history->temperature_history[i].data()), state->cells * sizeof(double));
    }


    // Write most recent grid
    for (int d = 0; d < 9; ++d) {
        in.read(reinterpret_cast<char*>(state->grid.g[d].data()), state->cells * sizeof(double));
    }

    // Go to the last frame of the sim
    if (history_count > 0) {
        state->current_step = history->time_history.back();
        state->temperatures = history->temperature_history.back();
    }

    in.close();
    return std::make_unique<T>(w, h, constantHeat, std::move(state), std::move(history));
}

// Main Writer: Kristian
// Reviewer: 
// Contributers: 
bool saveSimulation(const SimulationState& state, const SimulationHistory* history, const std::string& filepath);