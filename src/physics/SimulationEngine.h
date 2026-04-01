#ifndef PROJECT_2_2_GROUP_1_SIMULATIONENGINE_H
#define PROJECT_2_2_GROUP_1_SIMULATIONENGINE_H

#pragma once
#include "main.h"
#include <vector>

class SimulationEngine {
public: 
    Grid grid;
    Grid gridTemp;
    std::vector<double> temperatures;

    // State Checks
    int current_step;
    bool is_playing;
    double heat_spread;

    // Information for the stats
    std::vector<double> time_history;
    std::vector<double> max_temp_history;
    std::vector<double> min_temp_history;
    std::vector<std::vector<double>> temperature_history;

    // State history of the grid, so we can rewind
    std::vector<Grid> grid_history;

    SimulationEngine();

    // Step foward one frame
    void stepFoward();

    void stepBack();

    double getTotalEnergy() const;
};

#endif 