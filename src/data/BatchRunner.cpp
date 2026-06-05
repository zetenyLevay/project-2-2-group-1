#include "BatchRunner.h"
#include <thread>
#include <cmath>
#include <iostream>

// Main Writer: Kristian
// Reviewer: 
// Contributers: Berke
std::thread runSimulations(int width, int height, int temperature, bool constantHeatSource, int NumberOfSims, const std::string& filename) {
    return std::thread([=]() {
        for (int i = 0; i < NumberOfSims; ++i) {
            std::cout << "Starting Simulation " << i + 1 << " of " << NumberOfSims << std::endl;
            
            // Set temperature
            MAX_TEMP = temperature;
            
            LocalEngine engine(width, height, constantHeatSource);
            bool isComplete = false; // The simulation is complete once the hot spot and cold spot are equal 
            int expectedStep = 0;

            const SimulationHistory* history = engine.getReadOnlyHistory();

            while (!isComplete) {
                engine.stepFoward();
                expectedStep++;

                const SimulationState* state = engine.getState();

                // Make sure we are in sync with the thread
                while (state->current_step < expectedStep) {
                    std::this_thread::yield();
                    state = engine.getState();
                }

               double average = state->TempAvg;

                // Check if the average is above 30.0, stop the simulation if so
                if (average > 30.0) {
                    isComplete = true;

                    // Get the full path
                    std::string path = "../saves/" + filename;
                    if (NumberOfSims > 1) {
                        path += "-" + std::to_string(i + 1);
                    }
                    path += ".dat";

                    // Save the simulation
                    if (saveSimulation(*state, history, path)) {
                        std::cout << "Saved to: " << path << std::endl;
                    }
                    else {
                        std::cerr << "Failed to save to: " << path << std::endl;
                    }

                }
            }
        }
        std::cout << "All Simulations Complete" << std::endl;
    });
}