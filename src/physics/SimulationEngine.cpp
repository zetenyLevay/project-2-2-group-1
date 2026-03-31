#include "SimulationEngine.h"
#include <numeric>

SimulationEngine::SimulationEngine() {
    current_step = 0;
    is_playing = false;
    heat_spread = 1.0;
    temperatures.resize(CELLS, 20.0); // room temp assumption

    time_history.push_back(current_step);
    max_temp_history.push_back(MAX_TEMP);
    min_temp_history.push_back(ROOM_TEMP);
}

void SimulationEngine::stepFoward() {
    Collision(grid, gridTemp, heat_spread);
    stream(gridTemp, grid);

    double current_max = 0.0;
    double current_min = 100.0; // Assuming max 100 is possible

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
}

double SimulationEngine::getTotalEnergy() const {
    return std::accumulate(temperatures.begin(), temperatures.end(), 0.0);
}