#pragma once
#include "main.h"
#include <vector>
#include "../SimulationEngine.h"

class LocalEngine : public SimulationEngine {
public:
    LocalEngine(int w, int h, bool constantHeatSource);

    // Step foward one frame
    void stepFoward();

    void stepBack();

    void seekTo(int step);

    double getTotalEnergy() const;

    // Physics functions
    void Collision(double tauT,double TempAvg,double tauF, Grid& gridNew, Grid &gridOld);
    std::array<double, 3> getDensityAndVelocity(const Grid& grid,int idx);
    void Stream(Grid &gridOld, Grid &gridNew, std::vector<bool>& isRad);
};

std::unique_ptr<LocalEngine> loadLocalSimulation(const std::string& filepath);
bool saveSimulation(const SimulationState& state, const SimulationHistory& history, const std::string& filepath);