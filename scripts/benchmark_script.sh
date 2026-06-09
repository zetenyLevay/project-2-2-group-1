#!/bin/bash
# LBM Benchmark Script — cluster-ready
# Usage:
#   ./scripts/benchmark_script.sh                        # all 4 modes, local grid sizes
#   ./scripts/benchmark_script.sh --cluster              # adds 2048² and 4096²
#   ./scripts/benchmark_script.sh --mode cuda            # CUDA only
#   ./scripts/benchmark_script.sh --mode cpu,omp,cuda    # specific modes
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
TIMEOUT_SEC=600  # 10 min per benchmark run

MODES="cpu,omp,cuda"
GRID_SIZES=("32 32" "64 64" "128 128" "256 256" "512 512" "1024 1024")
WARMUP=50 STEPS=500 REPEATS=3

while [[ $# -gt 0 ]]; do
    case "$1" in
        --mode) MODES="$2"; shift 2 ;;
        --cluster)
            GRID_SIZES=("128 128" "256 256" "512 512" "1024 1024" "2048 2048" "4096 4096")
            STEPS=200  # less steps for big grids
            shift ;;
        --quick) GRID_SIZES=("128 128" "512 512" "1024 1024"); shift ;;
        *) echo "Unknown: $1"; exit 1 ;;
    esac
done

build_if_needed() {
    local name="$1" dir="$2"; shift 2
    if [ -f "$dir/benchmark_lbm" ]; then return; fi
    echo "=== Building $name ===" >&2
    cmake -DBUILD_GUI=OFF "$@" "$PROJECT_DIR" -B "$dir" >&2
    cmake --build "$dir" --target benchmark_lbm -j$(nproc) >&2
}

run_bench() {
    local exe="$1" engine="$2" w="$3" h="$4"
    local s=$STEPS cells=$((w * h))
    [ $cells -ge 1048576 ] && s=100      # 1024²
    [ $cells -ge 4194304 ] && s=50       # 2048²
    [ $cells -ge 16777216 ] && s=20      # 4096²

    timeout $TIMEOUT_SEC "$exe" --engine "$engine" \
        --width "$w" --height "$h" --steps "$s" \
        --warmup "$WARMUP" --repeat "$REPEATS" --csv 2>/dev/null || \
        echo "$engine,$w,$h,$cells,$s,FAIL,0,0,0"
}

echo "engine,width,height,cells,steps,mlups_mean,mlups_stddev,mlups_min,mlups_max"

# CPU serial
if [[ "$MODES" == *"cpu"* ]]; then
    DIR="$PROJECT_DIR/build-bench-cpu"
    build_if_needed "CPU serial" "$DIR" -DCMAKE_CUDA_COMPILER=""
    echo "# CPU serial" >&2
    for s in "${GRID_SIZES[@]}"; do read w h <<< "$s"
        OMP_NUM_THREADS=1 run_bench "$DIR/benchmark_lbm" cpu "$w" "$h"
    done
fi

# CPU OpenMP
if [[ "$MODES" == *"omp"* ]]; then
    DIR="$PROJECT_DIR/build-bench-omp"
    build_if_needed "CPU OpenMP" "$DIR" -DCMAKE_CUDA_COMPILER=""
    echo "# CPU OpenMP" >&2
    for s in "${GRID_SIZES[@]}"; do read w h <<< "$s"
        run_bench "$DIR/benchmark_lbm" openmp "$w" "$h"
    done
fi

# GPU CUDA
if [[ "$MODES" == *"cuda"* ]]; then
    DIR="$PROJECT_DIR/build-bench-cuda"
    CUDA_FLAG=""
    [ -n "${CUDA_ARCH:-}" ] && CUDA_FLAG="-DCMAKE_CUDA_ARCHITECTURES=$CUDA_ARCH"
    build_if_needed "GPU CUDA" "$DIR" $CUDA_FLAG
    echo "# GPU CUDA" >&2
    for s in "${GRID_SIZES[@]}"; do read w h <<< "$s"
        run_bench "$DIR/benchmark_lbm" cuda "$w" "$h"
    done
fi

echo "# Done" >&2
