//
// Created by levay on 3/23/2026.
//

#ifndef PROJECT_2_2_GROUP_1_MAIN_H
#define PROJECT_2_2_GROUP_1_MAIN_H

#include <vector>
#include <array>
#include <cmath>

// Default Width and Height
inline int defaultWidth = 500;
inline int defaultHeight = 500;

// Main Writer: Zétény
// Physics constants
// lb stands for lattice boltzmann unit
inline double MAX_TEMP = 55.0;
const double ROOM_TEMP = 20.0;
const int radX = 0;
const int radY = 0;

#pragma omp declare target
const double room_height = 2.5; // in meters
const double cells_height = (double)defaultHeight; // cells of 1d
const double delta_T = MAX_TEMP-ROOM_TEMP;
const double mach_number = 0.1; // for stability
const double rayliegh_num = 1e7;
const double prandtl_num = 0.71; // prandtl number of air
const double cs = 1/sqrt(3); // lattice speed of sound
const double cs2 = 1.0/3; // lattice speed of sound squared
const double lattice_thermal_diffusivity = (cells_height*mach_number*cs)/(sqrt(rayliegh_num*prandtl_num));
const double lattice_buoyancy = (mach_number*mach_number*cs2)/(delta_T*cells_height);
const double lattice_kinematic_viscosity = prandtl_num*lattice_thermal_diffusivity;
const double thermal_relaxation_time = 3*lattice_thermal_diffusivity+0.5;
const double density_relaxation_time = 3*lattice_kinematic_viscosity+0.5;
const double lattice_stefan_boltzmann = 1e-10;
const double real_viscosity = 1.5e-5;// kinematic viscosity of air at 20c in real life (m2/s)
const double delta_x = room_height/cells_height;
const double seconds_per_step = lattice_kinematic_viscosity*((delta_x*delta_x)/real_viscosity);

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