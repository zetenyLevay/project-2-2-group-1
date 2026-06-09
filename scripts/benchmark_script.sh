#!/bin/bash
# LBM Benchmark Script
# Runs all grid sizes × engine modes
# Usage: ./scripts/benchmark_sweep.sh [--cpu-only|--gpu-only|--quick]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

BUILD_CPU="$PROJECT_DIR/build-bench-cpu"
BUILD_OMP="$PROJECT_DIR/build-bench-omp"
BUILD_CUDA="$PROJECT_DIR/build-bench-cuda"

GRID_SIZES=(
    "32 32"
    "64 64"
    "128 128"
    "256 256"
    "512 512"
    "1024 1024"
)

# Quick mode uses fewer sizes
if [[ "${1:-}" == "--quick" ]]; then
    GRID_SIZES=("32 32" "128 128" "512 512")
fi

# Benchmark parameters
WARMUP=50
STEPS=500
REPEATS=3

# Output CSV header
echo "engine,width,height,cells,steps,mlups_mean,mlups_stddev,mlups_min,mlups_max"

run_bench() {
    local exe="$1"
    local engine="$2"
    local w="$3"
    local h="$4"

    # Reduce steps for very large grids to keep runtime reasonable
    local s=$STEPS
    local cells=$((w * h))
    if [ $cells -ge 1048576 ]; then  # 1024x1024
        s=200
    fi

    "$exe" --engine "$engine" --width "$w" --height "$h" \
           --warmup "$WARMUP" --steps "$s" --repeat "$REPEATS" --csv 2>/dev/null
}

# CPU Serial
if [[ "${1:-}" != "--gpu-only" ]]; then
    if [ ! -f "$BUILD_CPU/benchmark_lbm" ]; then
        echo "Building CPU serial..." >&2
        cmake -DBUILD_GUI=OFF -DCMAKE_DISABLE_FIND_PACKAGE_OpenMP=ON \
              "$PROJECT_DIR" -B "$BUILD_CPU" > /dev/null 2>&1
        cmake --build "$BUILD_CPU" --target benchmark_lbm -j$(nproc) > /dev/null 2>&1
    fi
    echo "# CPU serial" >&2
    for size in "${GRID_SIZES[@]}"; do
        read w h <<< "$size"
        run_bench "$BUILD_CPU/benchmark_lbm" cpu "$w" "$h"
    done
fi

# CPU OpenMP
if [[ "${1:-}" != "--gpu-only" ]]; then
    if [ ! -f "$BUILD_OMP/benchmark_lbm" ]; then
        echo "Building CPU OpenMP..." >&2
        cmake -DBUILD_GUI=OFF "$PROJECT_DIR" -B "$BUILD_OMP" > /dev/null 2>&1
        cmake --build "$BUILD_OMP" --target benchmark_lbm -j$(nproc) > /dev/null 2>&1
    fi
    echo "# CPU OpenMP" >&2
    for size in "${GRID_SIZES[@]}"; do
        read w h <<< "$size"
        run_bench "$BUILD_OMP/benchmark_lbm" openmp "$w" "$h"
    done
fi

# GPU CUDA
if [[ "${1:-}" != "--cpu-only" ]]; then
    if [ ! -f "$BUILD_CUDA/benchmark_lbm" ]; then
        echo "Building GPU CUDA..." >&2
        cmake -DBUILD_GUI=OFF "$PROJECT_DIR" -B "$BUILD_CUDA" > /dev/null 2>&1
        cmake --build "$BUILD_CUDA" --target benchmark_lbm -j$(nproc) > /dev/null 2>&1
    fi
    echo "# GPU CUDA" >&2
    for size in "${GRID_SIZES[@]}"; do
        read w h <<< "$size"
        run_bench "$BUILD_CUDA/benchmark_lbm" cuda "$w" "$h"
    done
fi

echo "# Done" >&2
