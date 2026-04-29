#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include "src/main/main.h"
#include "src/data/local/LocalEngine.h"

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace std;
using namespace std::chrono;

// Function to set number of OpenMP threads
void set_omp_threads(int threads) {
#ifdef _OPENMP
    omp_set_num_threads(threads);
#else
    (void)threads;
#endif
}

// Get current number of OpenMP threads
int get_omp_threads() {
#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}

double measure_simulation(int width, int height, int steps, int thread_count) {
    // Set thread count for this run
    set_omp_threads(thread_count);

    // Create engine
    LocalEngine engine(width, height);

    // Warmup
    for (int i = 0; i < 3; ++i) {
        engine.stepFoward();
        // Wait for computation to finish
        auto state = engine.getState();
        while (state->current_step < i + 1) {
            state = engine.getState();
        }
    }

    // Actual benchmark
    auto start = high_resolution_clock::now();

    for (int i = 0; i < steps; ++i) {
        engine.stepFoward();
        // Wait for computation to finish
        auto state = engine.getState();
        while (state->current_step < i + 3 + 1) { // 3 warm-up steps already done
            state = engine.getState();
        }
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    return duration.count() / 1e6; // Return time in seconds
}

void run_benchmark(int width, int height, int steps, int thread_count) {
    string label;
    if (thread_count == 1) {
        label = "Sequential";
    } else {
        label = "OpenMP (" + to_string(thread_count) + " threads)";
    }

    cout << "\n=== " << label << " Benchmark ===" << endl;
    cout << "Grid size: " << width << "x" << height << " (" << width*height << " cells)" << endl;
    cout << "Simulation steps: " << steps << endl;
    cout << "Thread count: " << thread_count << endl;

    // Run benchmark multiple times for more accurate results
    const int runs = 5;
    vector<double> times;

    for (int run = 0; run < runs; ++run) {
        cout << "Run " << run+1 << "/" << runs << "... ";
        cout.flush();

        double time = measure_simulation(width, height, steps, thread_count);
        times.push_back(time);

        cout << fixed << setprecision(3) << time << " seconds" << endl;
    }

    // Calculate statistics
    double sum = 0.0;
    double min_time = times[0];
    double max_time = times[0];

    for (double t : times) {
        sum += t;
        if (t < min_time) min_time = t;
        if (t > max_time) max_time = t;
    }

    double avg = sum / runs;
    double stddev = 0.0;
    for (double t : times) {
        stddev += (t - avg) * (t - avg);
    }
    stddev = sqrt(stddev / runs);

    cout << "\nResults:" << endl;
    cout << "  Average time: " << fixed << setprecision(3) << avg << " seconds" << endl;
    cout << "  Min time: " << fixed << setprecision(3) << min_time << " seconds" << endl;
    cout << "  Max time: " << fixed << setprecision(3) << max_time << " seconds" << endl;
    cout << "  Std deviation: " << fixed << setprecision(3) << stddev << " seconds" << endl;
    cout << "  Steps per second: " << fixed << setprecision(1) << steps / avg << endl;
    cout << "  Cells per second: " << fixed << setprecision(1) << (steps * width * height) / avg << endl;
    cout << "  Speedup vs sequential: ";

    // We'll calculate speedup later when we have sequential results
    static double sequential_time = 0.0;
    if (thread_count == 1) {
        sequential_time = avg;
        cout << "N/A (baseline)" << endl;
    } else if (sequential_time > 0) {
        double speedup = sequential_time / avg;
        cout << fixed << setprecision(2) << speedup << "x" << endl;
    } else {
        cout << "N/A (run sequential first)" << endl;
    }
}

int main(int argc, char* argv[]) {
    cout << "LBM Simulation Benchmark Tool" << endl;
    cout << "==============================" << endl;

#ifdef _OPENMP
    cout << "OpenMP is ENABLED" << endl;
    cout << "Max available threads: " << omp_get_max_threads() << endl;
#else
    cout << "OpenMP is NOT AVAILABLE (compiled without OpenMP support)" << endl;
#endif

    // Default test configurations
    vector<pair<int, int>> grid_sizes = {
        {50, 50},    // 2500 cells
        {100, 100},  // 10000 cells
        {200, 200}   // 40000 cells
    };

    int steps = 100; // Number of simulation steps per benchmark

    // Thread configurations to test
    vector<int> thread_configs;

    // Check command line arguments
    bool test_sequential_only = false;
    bool test_openmp_only = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sequential") == 0) {
            test_sequential_only = true;
        } else if (strcmp(argv[i], "--openmp") == 0) {
            test_openmp_only = true;
        } else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
            steps = atoi(argv[++i]);
        }
    }

    // Determine thread configurations to test
    if (test_sequential_only) {
        thread_configs = {1};
    } else if (test_openmp_only) {
        thread_configs = {get_omp_threads()};
    } else {
        // Test both sequential and parallel
        thread_configs = {1, get_omp_threads()};
    }

    for (const auto& size : grid_sizes) {
        int width = size.first;
        int height = size.second;

        for (int threads : thread_configs) {
            run_benchmark(width, height, steps, threads);
        }
    }

    cout << "\n=== Benchmark Complete ===" << endl;
    cout << "\nUsage notes:" << endl;
    cout << "  ./benchmark_lbm                    - Run both sequential and OpenMP" << endl;
    cout << "  ./benchmark_lbm --sequential       - Run sequential only" << endl;
    cout << "  ./benchmark_lbm --openmp           - Run OpenMP only" << endl;
    cout << "  ./benchmark_lbm --steps N          - Run N steps per test (default: 100)" << endl;
    cout << "  OMP_NUM_THREADS=N ./benchmark_lbm  - Set OpenMP thread count" << endl;

    return 0;
}