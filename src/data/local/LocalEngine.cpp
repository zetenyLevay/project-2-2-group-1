#include "LocalEngine.h"
#include <numeric>

// Initialized on main thread
LocalEngine::LocalEngine() {
    auto initialState = std::make_shared<SimulationState>();

    initialState->current_step = 0;
    initialState->heat_spread = 1.0;
    initialState->temperatures.resize(CELLS, 20.0); // room temp assumption
    initialState->temperatures[getIndex(0,0)] = MAX_TEMP; // Set heat source 

    // Initialize Grid 
    for (int i = 0; i < CELLS; i++) {
        for (int d = 0; d < 9; ++d) {
            initialState->grid.g[d][i] = weights[d] * initialState->temperatures[i];
        }
    }

    initialState->time_history.push_back(initialState->current_step);
    initialState->max_temp_history.push_back(MAX_TEMP);
    initialState->min_temp_history.push_back(ROOM_TEMP);
    initialState->temperature_history.push_back(initialState->temperatures);
    initialState->grid_history.push_back(initialState->grid);

    this->thread = std::make_unique<ReusableThread>(initialState);
}

// Mutates: grid, temperatures, current_step, time_history, max_temp_history, min_temp_history, temperature_history, grid_history
void LocalEngine::stepFoward() {
    thread->submitTask([this](SimulationState& state) {
            Grid gridTemp;
            Collision(state.grid, gridTemp, state.heat_spread);

            stream(gridTemp, state.grid);

            double current_max = ROOM_TEMP;
            double current_min = MAX_TEMP; // Assuming max 100 is possible

            for (int i = 0; i < CELLS; i++) {
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
    });
}

double LocalEngine::getTotalEnergy() const {
    // Can't really make this run on a seperate thread without changing the function signature.
    auto state = thread->getState();
    return std::accumulate(state->temperatures.begin(), state->temperatures.end(), 0.0);
}