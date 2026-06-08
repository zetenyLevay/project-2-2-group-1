#include "BatchRunner.h"
#include <thread>
#include <cmath>
#include <iostream>
#include <chrono>

#if CUDA_AVAILABLE == 1
#include "local/gpu/CUDA/LocalCUDAEngine.cuh"
#endif

// Main Writer: Kristian
// Reviewer:
// Contributers:
std::thread runSimulations(int width, int height, int temperature, bool constantHeatSource, int NumberOfSims, const std::string& filename) {
    return std::thread([=]() {
        for (int i = 0; i < NumberOfSims; ++i) {
            std::cout << "Starting Simulation " << i + 1 << " of " << NumberOfSims << std::endl;

            // Set temperature
            MAX_TEMP = temperature;

#if CUDA_AVAILABLE == 1
            LocalCUDAEngine engine(width, height, constantHeatSource);
            engine.batchMode = true;
#else
            LocalEngine engine(width, height, constantHeatSource);
#endif
            bool isComplete = false;
            int expectedStep = 0;

            const SimulationHistory* history = engine.getReadOnlyHistory();

            int stepCount = 0;
            const int BENCH_STEPS = 100;
            auto t_start = std::chrono::steady_clock::now();
            while (!isComplete && stepCount < BENCH_STEPS) {
                engine.stepFoward();
                expectedStep++;

                const SimulationState* state = engine.getState();

                // Make sure we are in sync with the thread
                while (state->current_step < expectedStep) {
                    std::this_thread::yield();
                    state = engine.getState();
                }

                stepCount++;
                double maxTemp = history->max_temp_history.back();
                double minTemp = history->min_temp_history.back();

                // The effective equilibrium
                if (std::abs(maxTemp - minTemp) < 0.1) {
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
            auto t_end = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(t_end - t_start).count();
            std::cout << "Benchmark: " << stepCount << " steps in " << elapsed
                      << "s = " << (stepCount / elapsed) << " steps/sec"
                      << " = " << (elapsed * 1000.0 / stepCount) << " ms/step" << std::endl;
        }
        std::cout << "All Simulations Complete" << std::endl;
    });
}