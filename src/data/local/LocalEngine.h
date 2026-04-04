#pragma once
#include "main.h"
#include <vector>
#include "../SimulationEngine.h"

class LocalEngine : public SimulationEngine {
public:
    LocalEngine();

    // Step foward one frame
    void stepFoward();

    void stepBack();

    double getTotalEnergy() const;
};