#include "SimulationEngine.h"
#include <numeric>

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