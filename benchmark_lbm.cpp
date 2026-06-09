// LBM Benchmark 
// Measures MLUPS across CPU serial, CPU OpenMP, GPU CUDA
//
// Usage:
//   ./benchmark_lbm --engine cpu|openmp|cuda --width W --height H [--steps N] [--warmup M] [--repeat R]
//
// Output: one line per run with MLUPS, then summary with mean ± stddev

#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "src/main/main.h"
#include "src/data/local/LocalEngine.h"
#include "src/thread/ReusableThread.h"

#if CUDA_AVAILABLE == 1
#include "src/data/local/gpu/CUDA/LocalCUDAEngine.cuh"
#include "src/data/local/gpu/CUDA/LocalCUDAEngineAoS.cuh"
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace std;
using namespace std::chrono;

enum class EngineType { CPU, OPENMP, CUDA, CUDA_AOS };

struct BenchmarkConfig {
    EngineType engine = EngineType::CPU;
    int width = 128;
    int height = 128;
    int warmup_steps = 50;
    int bench_steps = 500;
    int repeats = 3;
    bool csv_output = false;
    bool no_physics = false;  // skip radiation/heat/reduce for raw LBM comparison
};

struct BenchmarkResult {
    double time_seconds;
    double mlups;
};

// Engine factory

static unique_ptr<SimulationEngine> create_engine(EngineType type, int w, int h) {
    const bool constHeat = true;
    switch (type) {
        case EngineType::CPU:
        case EngineType::OPENMP:
            return make_unique<LocalEngine>(w, h, constHeat);
        case EngineType::CUDA:
#if CUDA_AVAILABLE == 1
            {
                auto e = make_unique<LocalCUDAEngine>(w, h, constHeat);
                e->batchMode = true;
                return e;
            }
#else
            cerr << "CUDA not available in this build." << endl;
            exit(1);
#endif
        case EngineType::CUDA_AOS:
#if CUDA_AVAILABLE == 1
            {
                auto e = make_unique<LocalCUDAEngineAoS>(w, h, constHeat);
                e->batchMode = true;
                return e;
            }
#else
            cerr << "CUDA not available in this build." << endl;
            exit(1);
#endif
        default:
            cerr << "Unknown engine type." << endl;
            exit(1);
    }
}

// Run N steps and measure wall time

BenchmarkResult run_steps(SimulationEngine& engine, int steps, int expected_start) {
    auto t0 = high_resolution_clock::now();

    for (int i = 0; i < steps; ++i) {
        engine.stepFoward();
        // Wait for compute thread to finish
        auto state = engine.getState();
        while (state->current_step < expected_start + i + 1) {
            this_thread::yield();
            state = engine.getState();
        }
    }

    auto t1 = high_resolution_clock::now();
    double elapsed = duration<double>(t1 - t0).count();

    BenchmarkResult result;
    result.time_seconds = elapsed;
    return result;
}

// ---- Full benchmark run (warmup + timed) ----

BenchmarkResult benchmark_run(const BenchmarkConfig& cfg) {
    auto engine = create_engine(cfg.engine, cfg.width, cfg.height);
    int cells = cfg.width * cfg.height;

    // Warmup (GPU JIT + cache warmup)
    if (cfg.warmup_steps > 0) {
        run_steps(*engine, cfg.warmup_steps, 0);
    }

    // Timed run
    auto result = run_steps(*engine, cfg.bench_steps, cfg.warmup_steps);
    result.mlups = (cells * (double)cfg.bench_steps) / (result.time_seconds * 1e6);

    // Shut down compute thread cleanly
    engine->thread->terminate();
    return result;
}

// ---- CLI parsing ----

BenchmarkConfig parse_args(int argc, char* argv[]) {
    BenchmarkConfig cfg;
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--engine" && i + 1 < argc) {
            string val = argv[++i];
            if (val == "cpu")      cfg.engine = EngineType::CPU;
            else if (val == "openmp") cfg.engine = EngineType::OPENMP;
            else if (val == "cuda")      cfg.engine = EngineType::CUDA;
            else if (val == "cuda-aos")  cfg.engine = EngineType::CUDA_AOS;
            else { cerr << "Unknown engine: " << val << endl; exit(1); }
        } else if (arg == "--width" && i + 1 < argc) {
            cfg.width = atoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            cfg.height = atoi(argv[++i]);
        } else if (arg == "--steps" && i + 1 < argc) {
            cfg.bench_steps = atoi(argv[++i]);
        } else if (arg == "--warmup" && i + 1 < argc) {
            cfg.warmup_steps = atoi(argv[++i]);
        } else if (arg == "--repeat" && i + 1 < argc) {
            cfg.repeats = atoi(argv[++i]);
        } else if (arg == "--csv") {
            cfg.csv_output = true;
        }
    }
    return cfg;
}

const char* engine_name(EngineType e) {
    switch (e) {
        case EngineType::CPU:    return "CPU";
        case EngineType::OPENMP:
#ifdef _OPENMP
            return "OpenMP";
#else
            return "CPU";
#endif
        case EngineType::CUDA:      return "CUDA";
        case EngineType::CUDA_AOS:  return "CUDA_AoS";
        default:                    return "?";
    }
}

// ---- Main ----

int main(int argc, char* argv[]) {
    auto cfg = parse_args(argc, argv);

    int cells = cfg.width * cfg.height;

    if (!cfg.csv_output) {
        cerr << "=== LBM Benchmark ===" << endl;
        cerr << "Engine: " << engine_name(cfg.engine) << endl;
        cerr << "Grid: " << cfg.width << "x" << cfg.height
             << " (" << cells << " cells)" << endl;
        cerr << "Warmup: " << cfg.warmup_steps << " steps" << endl;
        cerr << "Bench: " << cfg.bench_steps << " steps" << endl;
        cerr << "Repeats: " << cfg.repeats << endl;
        cerr << "=========================" << endl;
    }

    vector<BenchmarkResult> results;
    for (int r = 0; r < cfg.repeats; ++r) {
        auto res = benchmark_run(cfg);
        results.push_back(res);
        if (!cfg.csv_output) {
            cerr << "  Run " << (r + 1) << ": "
                 << fixed << setprecision(3) << res.time_seconds << " s  "
                 << fixed << setprecision(2) << res.mlups << " MLUPS" << endl;
        }
    }

    // Statistics
    double sum_t = 0, sum_m = 0;
    double min_m = results[0].mlups, max_m = results[0].mlups;
    for (auto& r : results) {
        sum_t += r.time_seconds;
        sum_m += r.mlups;
        if (r.mlups < min_m) min_m = r.mlups;
        if (r.mlups > max_m) max_m = r.mlups;
    }
    double avg_t = sum_t / cfg.repeats;
    double avg_m = sum_m / cfg.repeats;

    double stddev_m = 0;
    for (auto& r : results) {
        double d = r.mlups - avg_m;
        stddev_m += d * d;
    }
    stddev_m = sqrt(stddev_m / cfg.repeats);

    if (cfg.csv_output) {
        // CSV row: engine,width,height,cells,steps,mlups_mean,mlups_stddev,mlups_min,mlups_max
        cout << engine_name(cfg.engine) << ","
             << cfg.width << "," << cfg.height << ","
             << cells << "," << cfg.bench_steps << ","
             << fixed << setprecision(2)
             << avg_m << "," << stddev_m << ","
             << min_m << "," << max_m << endl;
    } else {
        cerr << "-------------------------" << endl;
        cerr << "Results: " << fixed << setprecision(2)
             << avg_m << " ± " << stddev_m << " MLUPS"
             << "  [min=" << min_m << " max=" << max_m << "]"
             << "  avg_time=" << fixed << setprecision(3) << avg_t << " s" << endl;
    }

    return 0;
}
