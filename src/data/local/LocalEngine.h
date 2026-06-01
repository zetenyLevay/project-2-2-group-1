#pragma once
#include "main.h"
#include <vector>
#include "../SimulationEngine.h"

class LocalEngine : public SimulationEngine {
public:
    LocalEngine(int w, int h, bool constantHeatSource);

    // Control Simulation functions
    void stepFoward();
    void stepBack();
    void seekTo(int step);

    // Physics functions
    void Collision(double tauT,double TempAvg,double tauF, Grid& gridNew, Grid &gridOld);
    void Stream(Grid &gridOld, Grid &gridNew, std::vector<bool>& isRad);
    void Radiation(SimulationState& state);

    // Helper functions
    std::array<double, 3> getDensityAndVelocity(const Grid& grid,int idx);
    double getTotalEnergy() const;
};

std::unique_ptr<LocalEngine> loadLocalSimulation(const std::string& filepath);
bool saveSimulation(const SimulationState& state, const SimulationHistory& history, const std::string& filepath);