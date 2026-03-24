//
// Created by levay on 3/23/2026.
//

#ifndef PROJECT_2_2_GROUP_1_MAIN_H
#define PROJECT_2_2_GROUP_1_MAIN_H

#include <vector>
// 1. Shared Constants
const int WIDTH = 3;
const int HEIGHT = 2;
const int CELLS = WIDTH * HEIGHT;

// Directions
const int cx[9] = {0,1,0,-1,0,1,-1,1,-1};
const int cy[9] = {0,0,1,0,-1,1,-1,-1,1};

// 2. Shared Data Structure (Structure of Arrays)
struct Grid {
    std::vector<double> g0,g1,g2,g3,g4,g5,g6,g7,g8;
    Grid() :
        g0(CELLS, 0.0), g1(CELLS, 0.0), g2(CELLS, 0.0), g3(CELLS, 0.0), g4(CELLS, 0.0),
        g5(CELLS, 0.0), g6(CELLS, 0.0), g7(CELLS, 0.0), g8(CELLS, 0.0) {};
};

// 3. Function Declarations
int getIndex(int x, int y);
void stream(const Grid& gridOld, Grid& gridNew);

#endif //PROJECT_2_2_GROUP_1_MAIN_H