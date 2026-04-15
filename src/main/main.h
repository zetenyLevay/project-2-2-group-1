//
// Created by levay on 3/23/2026.
//

#ifndef PROJECT_2_2_GROUP_1_MAIN_H
#define PROJECT_2_2_GROUP_1_MAIN_H

#include <vector>
#include <array>



// Main Writer: Zétény
// Physics constants
const double MAX_TEMP = 10000.0;
const double ROOM_TEMP = 20.0;
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

// 2. Shared Data Structure (Structure of Arrays)
struct Grid {
    std::array<std::vector<double>, 9> g,f;
    Grid(int cells) {
        for (int i = 0; i < 9; ++i) {
            g[i].resize(cells, 0.0);
            f[i].resize(cells,0.0);
        }
    }
};

#endif //PROJECT_2_2_GROUP_1_MAIN_H