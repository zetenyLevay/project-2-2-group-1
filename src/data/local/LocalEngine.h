#pragma once
#include "main.h"
#include <vector>
#include "../SimulationEngine.h"

class LocalEngine : public SimulationEngine {
public:
    LocalEngine(int w, int h);

    // Step foward one frame
    void stepFoward();

    void stepBack();

    void seekTo(int step);

    double getTotalEnergy() const;

    // Physics functions
    void Collision(double heat_spread, Grid& gridTemp, const Grid grid);
    void Stream(const Grid gridTemp, Grid &grid);
};

enum SaveType {
    NECESSARY,
    COMPLETE
};

std::unique_ptr<LocalEngine> loadLocalSimulation(const std::string& filepath);
bool saveSimulation(const SimulationState state, const std::string& filepath, const SaveType saveType = SaveType::COMPLETE);