//
// Created by levay on 3/23/2026.
//

#include "main.h"

const int WIDTH = 3;
const int HEIGHT = 2;
const int CELLS = WIDTH * HEIGHT;

const double MAX_TEMP = 100.0; // temp of top left cell
const double ROOM_TEMP = 20.0;

// relaxation time for temperature spread
const double heat_spread = 1.0;

// weights of directions
const double w0 = 4/9; // rest direction (itself)
const double w1_4 = 1/9; // cardinal directions
const double w5_9 = 1/36; // diagnol directions
const double weights[9] = {w0, w1_4,w1_4,w1_4,w1_4,w5_9,w5_9,w5_9,w5_9};

// directions
const int cx[9] = {0,1,0,-1,0,1,-1,1,-1};
const int cy[9] = {0,0,1,0,-1,1,-1,-1,1};

// speed of spread
const double u = 1.0;

int main() {
    return 0;
}