#pragma once
#include "main.h"
#include <vector>
#include "../SimulationEngine.h"

// Computes density, ux, uy from a flattened f array (direction-major, size = 9 * cells).
#pragma omp declare target
inline std::array<double, 3> computeDensityAndVelocity(const double* f, int cells, int idx) {
    double density = 0.0;
    double ux = 0.0;
    double uy = 0.0;
    for (int d = 0; d < 9; ++d) {
        double val = f[d * cells + idx];
        density += val;
        ux += val * cx[d];
        uy += val * cy[d];
    }
    if (density != 0.0) {
        ux /= density;
        uy /= density;
    }
    return {density, ux, uy};
}
#pragma omp end declare target

class LocalEngine : public SimulationEngine {
public:
    LocalEngine(int w, int h, bool constantHeatSource);

    // Step foward one frame
    void stepFoward();

    void stepBack();

    void seekTo(int step);

    double getTotalEnergy() const;

    // Physics functions
    void Radiation(SimulationState& state);
    void Collision(double tauT,double TempAvg,double tauF, Grid& gridNew, Grid &gridOld);
    void Stream(Grid &gridOld, Grid &gridNew, std::vector<bool>& isRad, std::vector<bool>& isWindow, SimulationState& state);

    std::array<double, 3> getDensityAndVelocity(const Grid& grid,int idx);
};

std::unique_ptr<LocalEngine> loadLocalSimulation(const std::string& filepath);
bool saveSimulation(const SimulationState& state, const SimulationHistory& history, const std::string& filepath);