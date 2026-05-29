#pragma once
#include "main.h"
#include <vector>
#include "../SimulationEngine.h"

class LocalEngine : public SimulationEngine {
    public:
        LocalEngine(int w, int h, bool constantHeatSource);
        LocalEngine(int w, int h, bool constantHeatSource, std::unique_ptr<SimulationState> initialState, std::unique_ptr<SimulationHistory> initialHistory);

        // Step foward one frame
        void stepFoward();

        void stepBack();

        void seekTo(int step);

        double getTotalEnergy() const;

    private:
        // Physics functions
        void Collision(double tauT,double TempAvg,double tauF, Grid& gridNew, const Grid &gridOld);
        std::array<double, 3> getDensityAndVelocity(const Grid& grid,int idx);
        void Stream(Grid &gridOld, Grid &gridNew);
};