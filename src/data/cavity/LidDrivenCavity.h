#pragma once
#include "main.h"
#include "../SimulationEngine.h"
#include <string>

class LidDrivenCavity : public SimulationEngine {
public:
    LidDrivenCavity(int N, double Re, double U_lid = 0.1);

    void stepFoward() override;
    void stepBack() override;
    void seekTo(int step) override;

    double getResidual();
    bool hasConverged(double tolerance = 1e-8, int minSteps = 1000);

    // Run to steady state and save velocity profiles to CSV
    void runUntilConvergence(double tolerance = 1e-8, int maxSteps = 100000, int minSteps = 1000);
    void extractProfiles(const std::string& filepath);

    Grid gridTemp;
    double tau;        // BGK relaxation time
    double lid_U;      // lid velocity (lattice units)
    double Re_number;  // Reynolds number
    int N;             // grid size (N x N)

private:
    void Collision(double tau, Grid& gridNew, const Grid& gridOld);
    void Stream(const Grid& gridOld, Grid& gridNew, double U_lid);
};

// CLI function
void runCavityBenchmark(int N, double Re, double U_lid = 0.1, double tolerance = 1e-8);
