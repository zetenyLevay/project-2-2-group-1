#pragma once
#include "main.h"

struct SimulationState;

#include "../thread/ReusableThread.h"
#include <vector>
#include <string>
#include <memory>

// Line between heat source and wall (used for calculating how radiation transfers)
struct ViewFactor {
    int sourceIdx;
    int targetIdx;
    double factor;
};

struct SimulationState {
    int width, height, cells;
    Grid grid;
    std::vector<double> temperatures;
    int heatSourceW;
    int heatSourceH;
    int windowW;
    int windowH;
    std::vector<bool> isRad;
    std::vector<bool> isWindow;
    std::vector<int> heatSources;
    std::vector<int> windowSources;
    bool isConstantHeatSource;
    bool isConstantWindow;

    std::vector<int> boundaryCells;
    std::vector<ViewFactor> viewFactors;
    std::vector<std::vector<double>> localFluxCache;
    std::vector<double> t4;

    // Make grid default size 0.
    SimulationState(): grid(0) {}

    // State Checks
    int current_step;
    double heat_spread;
    double tauT; //relaxation time temeprature
    double viscosity;
    double tauF; //relaxation time fluid
    double TempAvg;
};

struct SimulationHistory {
    // Information for the stats
    std::vector<double> time_history;
    std::vector<double> max_temp_history;
    std::vector<double> min_temp_history;
    std::vector<std::vector<double>> temperature_history;
    std::vector<double> convectionOutput;
    std::vector<double> radiationOutput;
};

class SimulationEngine {
public:
    int width, height, cells;

    std::unique_ptr<ReusableThread> thread;

    std::shared_ptr<const SimulationState> getState();

    std::shared_ptr<SimulationState> getMutableState();

    SimulationHistory history;

    SimulationEngine(int w, int h);

    const int getIndex(int x, int y);

    virtual ~SimulationEngine() = default;

    // Step foward one frame
    virtual void stepFoward() = 0;

    virtual void stepBack() = 0;

    virtual void seekTo(int step) = 0;

    bool getAutoPlayStatus();
    void setAutoPlayStatus(bool status);

    private:
        bool autoPlay = false;
};

enum DataSource {
    LOCAL
};