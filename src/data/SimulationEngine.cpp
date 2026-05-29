#include "SimulationEngine.h"
#include "../thread/ReusableThread.h"
#include <stdexcept>


SimulationEngine::SimulationEngine(int w, int h) : width(w), height(h), cells(w * h) {};

bool SimulationEngine::getAutoPlayStatus() {
    return this->autoPlay;
}

void SimulationEngine::setAutoPlayStatus(bool status) {
    this->autoPlay = status;

    if (status) {
        this->stepFoward();
    }
}

/**
 * Gets the current state of the simulation.
 */
const SimulationState* SimulationEngine::getState() {
    return this->thread->getState();
}

// Main Writer: Gecenio
// Reviewer: 
// Contributers: Cosmin
int SimulationEngine::getIndex(int x, int y) {
    if(y>=0 && y<height)
    {
        if(x>=0 && x<width)
        {
            return y * this->width + x;
        }
    }
    throw std::out_of_range("Index out of bounds");
}

const SimulationHistory* SimulationEngine::getReadOnlyHistory() {
    return this->history.get();
}

SimulationEngine::~SimulationEngine() = default;

// Main Writer: Kristian
// Reviewer: 
// Contributers: 
bool saveSimulation(const SimulationState& state, const SimulationHistory* history, const std::string& filepath) {
    std::filesystem::path pathObj(filepath);
    std::filesystem::path dir = pathObj.parent_path();

    if (!dir.empty() && !std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
        std::cout << "Created missing saves folder" << std::endl;
    }

    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) return false;

    // Write width and height
    out.write(reinterpret_cast<const char*>(&state.width), sizeof(state.width));
    out.write(reinterpret_cast<const char*>(&state.height), sizeof(state.height));

    // Write whether the heat is constant
    out.write(reinterpret_cast<const char*>(&state.isConstantHeatSource), sizeof(state.isConstantHeatSource));

    // Write history length
    size_t history_count = history->time_history.size();
    out.write(reinterpret_cast<const char*>(&history_count), sizeof(history_count));

    // Write basic history information
    out.write(reinterpret_cast<const char*>(history->time_history.data()), history_count * sizeof(double));
    out.write(reinterpret_cast<const char*>(history->max_temp_history.data()), history_count * sizeof(double));
    out.write(reinterpret_cast<const char*>(history->min_temp_history.data()), history_count * sizeof(double));

    // Write temperature history
    for (size_t i = 0; i < history_count; ++i) {
        out.write(reinterpret_cast<const char*>(history->temperature_history[i].data()), state.cells * sizeof(double));
    }

    // Get most recent grid
    for (int d = 0; d < 9; ++d) {
        out.write(reinterpret_cast<const char*>(state.grid.g[d].data()), state.cells * sizeof(double));
    }

    out.close();
    return true;
}