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
    LocalEngine(int w, int h, bool constantHeatSource, std::unique_ptr<SimulationState> initialState, std::unique_ptr<SimulationHistory> initialHistory);

    // Step foward one frame
    void stepFoward();

    void stepBack();

    void seekTo(int step);

    double getTotalEnergy() const;

    // Physics functions
    void Radiation(const SimulationState& previousState, SimulationState& nextState, Grid& targetGrid);
    void Collision(double tauT,double TempAvg,double tauF, double lattice_buoyancy, Grid& gridNew, const Grid &gridOld);
    void Stream(Grid &gridOld, Grid &gridNew, const std::vector<bool>& isRad, SimulationState& state);

    std::array<double, 3> getDensityAndVelocity(const Grid& grid,int idx);

    private:
        void initPhysics(SimulationState& state);
};

std::unique_ptr<LocalEngine> loadLocalSimulation(const std::string& filepath);
bool saveSimulation(const SimulationState& state, const SimulationHistory* history, const std::string& filepath);