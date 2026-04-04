#pragma once
#include "main.h"

struct SimulationState;

#include "../thread/ReusableThread.h"
#include <vector>

struct SimulationState {
    Grid grid;
    std::vector<double> temperatures;

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
    std::unique_ptr<ReusableThread> thread;

    std::shared_ptr<const SimulationState> getState();

    SimulationEngine() = default;
    virtual ~SimulationEngine() = default;

    // Step foward one frame
    virtual void stepFoward() = 0;

    virtual void stepBack() = 0;

    virtual double getTotalEnergy() const = 0;
};

enum DataSource {
    LOCAL
};