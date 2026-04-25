//
// Created by levay on 3/23/2026.
//

#ifndef PROJECT_2_2_GROUP_1_MAIN_H
#define PROJECT_2_2_GROUP_1_MAIN_H

#include <vector>
#include <array>
#include <cmath>


// Main Writer: Zétény
// Physics constants
// lb stands for lattice boltzmann unit
const double MAX_TEMP = 10000.0;
const double ROOM_TEMP = 20.0;
const double room_height = 4.0; // in meters
const double example_cells = 50; // cells of 1d
const double room_width = 4.0; // in meters
const double conversion_factor = room_width/example_cells; // conversion factor of length to lb_length
const double conversion_factor_squared = pow((conversion_factor),2);
const double cs2= 1.0/3.0; //lattice constant speed of sound squared
const double relaxation_time = 0.9;
const double heat_spread = cs2 * (relaxation_time - 0.5);
const double real_viscosity = 1.5 * pow(10,-5); // kinematic viscosity of air at 20c in real life (m2/s)
const double real_time_conversion = ((relaxation_time - 0.5)/3)*(conversion_factor_squared/real_viscosity); // 1 time step is equivalent to this in real time
const double real_velocity = 0.1; // velocity of air in meters/second
const double velocity_conversion = (conversion_factor/real_time_conversion); // 1 lb_velocity multiplied by this is real velocity
const double max_velocity = real_velocity/(velocity_conversion); // max lb velocity
const double real_reynold = (real_velocity*room_height)/real_viscosity; // reynolds number
const double lb_viscosity = (relaxation_time - 0.5) / 3.0;
const double lb_reynold = ((max_velocity*example_cells)/lb_viscosity);



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