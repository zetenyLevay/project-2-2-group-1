//
// Created by levay on 3/23/2026.
//

#include "main.h"
#include <iostream>
#include <vector>
#include <iomanip>


const double MAX_TEMP = 100.0; // temp of top left cell
const double ROOM_TEMP = 20.0;

// relaxation time for temperature spread
const double heat_spread = 1.0;

// weights of directions
const double w0 = 4/9; // rest direction (itself)
const double w1_4 = 1/9; // cardinal directions
const double w5_9 = 1/36; // diagnol directions
const double weights[9] = {w0, w1_4,w1_4,w1_4,w1_4,w5_9,w5_9,w5_9,w5_9};



// Helper to print the grid cleanly (ai)
void printTemperatures(const std::vector<double>& temps, int step) {
    std::cout << "--- Time Step " << step << " ---" << std::endl;
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            std::cout << std::fixed << std::setprecision(2) << std::setw(8) << temps[getIndex(x, y)];
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

int main() {
    Grid gridOld;
    Grid gridNew;

    std::vector<double> temperatures(CELLS,20.0);

    temperatures[getIndex(0,0)] = 100.0;

    for (int i = 1; i < CELLS; i++) {
        gridOld.g0[i] = weights[0] * temperatures[i];
        gridOld.g1[i] = weights[1] * temperatures[i];
        gridOld.g2[i] = weights[2] * temperatures[i];
        gridOld.g3[i] = weights[3] * temperatures[i];
        gridOld.g4[i] = weights[4] * temperatures[i];
        gridOld.g5[i] = weights[5] * temperatures[i];
        gridOld.g6[i] = weights[6] * temperatures[i];
        gridOld.g7[i] = weights[7] * temperatures[i];
        gridOld.g8[i] = weights[8] * temperatures[i];
    }

    printTemperatures(temperatures, 0);
    return 0;
}