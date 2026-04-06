#include "BatchRunner.h"
#include <thread>
#include <cmath>
#include <iostream>

void runSimulations(int width, int height, int NumberOfSims, const std::string& filename, SaveType saveType) {
    std::thread([=]() {
        for (int i = 0; i < NumberOfSims; ++i) {
            std::cout << "Starting Simulation " << i + 1 << " of " << NumberOfSims << std::endl;

            LocalEngine engine(width, height);
            bool isComplete = false; // The simulation is complete once the hot spot and cold spot are equal 
            int expectedStep = 0;

            while (!isComplete) {
                engine.stepFoward();
                expectedStep++;

                auto state = engine.getState();

                // Make sure we are in sync with the thread
                while (state->current_step < expectedStep) {
                    std::this_thread::yield();
                    state = engine.getState();
                }

                double maxTemp = state->max_temp_history.back();
                double minTemp = state->min_temp_history.back();

                // The effective equilibream, no need to check for until it is exactly equal
                if (std::abs(maxTemp - minTemp) < 0.1) {
                    isComplete = true;

                    // Get the full path
                    std::string path = "../saves/" + filename;
                    if (NumberOfSims > 1) {
                        path += "-" + std::to_string(i + 1);
                    }
                    path += ".dat";

                    // Save the simulation
                    if (saveSimulation(*state, path, saveType)) {
                        std::cout << "Saved to: " << path << std::endl;
                    }
                    else {
                        std::cerr << "Failed to save to: " << path << std::endl;
                    }

                }
            }
        }
        std::cout << "All Simulations Complete" << std::endl;
    }).detach();
}