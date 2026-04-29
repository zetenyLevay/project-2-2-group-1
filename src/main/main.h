//
// Created by levay on 3/23/2026.
//

#ifndef PROJECT_2_2_GROUP_1_MAIN_H
#define PROJECT_2_2_GROUP_1_MAIN_H

#include <vector>
#include <array>



// Main Writer: Zétény
// Physics constants
const double MAX_TEMP = 100.0;
const double ROOM_TEMP = 20.0;

#pragma omp declare target
const double cs2= 1.0/3.0; //lattice constant speed of sound

// Directions
const int cx[9] = {0,1,0,-1,0,1,-1,-1,1};
const int cy[9] = {0,0,1,0,-1,1,1,-1,-1};
const int inv[9] = {0,3,4,1,2,7,8,5,6}; // exact inverse direction in case a wall is hit

// Weights of directions
const double w0 = 4.0/9.0; // rest direction (itself)
const double w1_4 = 1.0/9.0; // cardinal directions
const double w5_9 = 1.0/36.0; // diagnol directions
const double weights[9] = {w0, w1_4,w1_4,w1_4,w1_4,w5_9,w5_9,w5_9,w5_9};
#pragma omp end declare target

// 2. Shared Data Structure (Structure of Arrays, GPU-friendly flat layout)
// g[d * cells + i] stores direction d at cell i — single contiguous allocation
// for efficient OpenMP target offloading (one map clause per field instead of 9).
struct Grid {
    int cells;
    std::vector<double> g;  // size = 9 * cells
    std::vector<double> f;  // size = 9 * cells

    Grid() : cells(0) {}
    Grid(int cells) : cells(cells), g(9 * cells, 0.0), f(9 * cells, 0.0) {}
};

#endif //PROJECT_2_2_GROUP_1_MAIN_H