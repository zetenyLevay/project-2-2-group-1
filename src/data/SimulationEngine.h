#pragma once
#include "main.h"

struct SimulationState;

#include "../thread/ReusableThread.h"
#include <vector>
#include <string>
#include <memory>

struct SimulationState {
    int width, height, cells;
    Grid grid;
    std::vector<double> temperatures;

    // Make grid default size 0.
    SimulationState(): grid(0) {}

    // State Checks
    int current_step;
    double heat_spread;

    // Information for the stats
    std::vector<double> time_history;
    std::vector<double> max_temp_history;
    std::vector<double> min_temp_history;
    std::vector<std::vector<double>> temperature_history;

    // State history of the grid, so we can rewind
    std::vector<Grid> grid_history;
};

class SimulationEngine {
public:
    int width, height, cells;

    std::unique_ptr<ReusableThread> thread;

    std::shared_ptr<const SimulationState> getState();

    std::shared_ptr<SimulationState> getMutableState();

    SimulationEngine(int w, int h);

    const int getIndex(int x, int y);

    virtual ~SimulationEngine() = default;

    // Step foward one frame
    virtual void stepFoward() = 0;

    virtual void stepBack() = 0;

    virtual void seekTo(int step) = 0;
};

enum DataSource {
    LOCAL
};