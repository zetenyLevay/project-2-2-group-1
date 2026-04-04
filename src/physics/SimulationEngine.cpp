#include "SimulationEngine.h"
#include <numeric>

SimulationEngine::SimulationEngine() {
    current_step = 0;
    is_playing = false;
    heat_spread = 1.0;
    viscosity = 0.3;
    temperatures.resize(CELLS, 20.0); // room temp assumption
    temperatures[getIndex(0,0)] = MAX_TEMP; // Set heat source 

    // Initialize Grid 
    for (int i = 0; i < CELLS; i++) {
        for (int d = 0; d < 9; ++d) {
            grid.g[d][i] = weights[d] * temperatures[i];

            grid.f[d][i] = weights[d] * 1.0;
        }
    }

    time_history.push_back(current_step);
    max_temp_history.push_back(MAX_TEMP);
    min_temp_history.push_back(ROOM_TEMP);
    temperature_history.push_back(temperatures);
    grid_history.push_back(grid);
}

void SimulationEngine::stepFoward() {
    FluidCollision(grid,gridTemp,viscosity);
    ThermalCollision(grid, gridTemp, heat_spread);
    stream(gridTemp, grid);

    double current_max = ROOM_TEMP;
    double current_min = MAX_TEMP; // Assuming max 100 is possible

    for (int i = 0; i < CELLS; i++) {
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
}

double SimulationEngine::getTotalEnergy() const {
    return std::accumulate(temperatures.begin(), temperatures.end(), 0.0);
}