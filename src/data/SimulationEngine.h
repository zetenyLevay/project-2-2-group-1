#pragma once
#include "main.h"

struct SimulationState;
class ReusableThread;


#include <vector>
#include <string>
#include <memory>

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
    std::vector<char> isRad;
    std::vector<int> heatSources;
    bool isConstantHeatSource;

    std::vector<int> boundaryCells;
    std::vector<int> cellToBoundary;  // maps cell index -> boundaryCells index, -1 if not boundary
    std::vector<ViewFactor> viewFactors;
    std::vector<std::vector<double>> localFluxCache;
    std::vector<double> t4;

    // Make grid default size 0
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

    const SimulationState* getState();

    SimulationEngine(int w, int h);

    int getIndex(int x, int y);

    virtual ~SimulationEngine();

    // Step foward one frame
    virtual void stepFoward() = 0;

    virtual void stepBack() = 0;

    virtual void seekTo(int step) = 0;

    bool getAutoPlayStatus();
    void setAutoPlayStatus(bool status);

    const SimulationHistory* getReadOnlyHistory();

    protected:
        std::unique_ptr<SimulationHistory> history;

    private:
        bool autoPlay = false;
};

enum DataSource {
    LOCAL,
    LOCAL_CUDA
};
