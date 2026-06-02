#include "LocalEngine.h"
#include "../../thread/ReusableThread.h"
#include <numeric>
#include <iostream>

// Initialized on main thread
// Main Writer: Berke/Kristian 
// Reviewer: 
// Contributers: 
LocalEngine::LocalEngine(int width, int height, bool constantHeatSource) : SimulationEngine(width, height) {
    auto initialState = std::make_unique<SimulationState>();

    initialState->width = width;
    initialState->height = height;

    auto cells = initialState->cells = width * height;

    initialState->grid = initialState->cells;

    initialState->current_step = 0;
    initialState->heat_spread = thermal_relaxation_time;
    initialState->viscosity = lattice_kinematic_viscosity;
    initialState->TempAvg=0.0;
    initialState->heatSource=getIndex(initialState->width/2,0); // Set heat source
    //relaxation times for heat_spread and visocsity
    //we are using 3 because we divide by cs2 which is 1/3
    initialState->tauF = initialState->viscosity*3 +0.5;
    initialState->tauT = initialState->heat_spread*3  +0.5;
    initialState->TempAvg = 0.0;
    initialState->heatSource = getIndex(initialState->width/2,0); // Set heat source
    initialState->isConstantHeatSource = constantHeatSource;
    initialState->temperatures.resize(cells, 20.0); // room temp assumption

    // Initialize Grid 
    for (int i = 0; i < cells; i++) {
        for (int d = 0; d < 9; ++d) {
            initialState->grid.g[d][i] = weights[d] * initialState->temperatures[i];
            initialState->grid.f[d][i] = weights[d] *1.0; //initializing the flow of the fluid 
        }
        initialState->TempAvg = initialState->TempAvg + initialState->temperatures[i];
    }
    initialState->TempAvg = initialState->TempAvg/cells;

    this->history = std::make_unique<SimulationHistory>();

    this->history->time_history.push_back(initialState->current_step);
    this->history->max_temp_history.push_back(MAX_TEMP);
    this->history->min_temp_history.push_back(ROOM_TEMP);
    this->history->temperature_history.push_back(initialState->temperatures);

    // Launch the compute thread.
    this->thread = std::make_unique<ReusableThread>(std::move(initialState));
}

LocalEngine::LocalEngine(int w, int h, bool constantHeatSource, std::unique_ptr<SimulationState> initialState, std::unique_ptr<SimulationHistory> initialHistory): SimulationEngine(w, h) {
    this->history = std::move(initialHistory);
    this->thread = std::make_unique<ReusableThread>(std::move(initialState));
}

// Main Writer: Gecenio 
// Reviewer: 
// Contributers: Kristian, Berke
void LocalEngine::stepFoward() {
    thread->submitTask([this](const SimulationState& previousState, SimulationState& nextState) {
        // If already calculated just set the grid and temperatures again 
        if (previousState.current_step < this->history->temperature_history.size() - 1) {
            nextState.current_step = previousState.current_step + 1;
            nextState.temperatures = this->history->temperature_history[nextState.current_step];

            // Auto play check
            if (this->getAutoPlayStatus()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                this->stepFoward();
            }

            return;
        }

        // update heatSource back it its oringinal temperature
        if (previousState.isConstantHeatSource) {
            nextState.temperatures[previousState.heatSource] = MAX_TEMP;
            for (int d = 0; d < 9; ++d) {
                nextState.grid.g[d][previousState.heatSource] = weights[d] * previousState.temperatures[previousState.heatSource];
                nextState.grid.f[d][previousState.heatSource] = weights[d] *1.0; //a constant heat source should not have movement. It should radiate heat evenly
            }
        }

        Grid gridTemp(previousState.cells);
        this->Collision(previousState.tauT, previousState.TempAvg, previousState.tauF, gridTemp, previousState.grid);

        this->Stream(gridTemp, nextState.grid);

        double current_max = ROOM_TEMP;
        double current_min = MAX_TEMP;
        nextState.TempAvg=0.0;
        for (int i = 0; i < cells; i++) {
            double temp = 0.0;

            for (int d = 0; d < 9; ++d) {
                temp += nextState.grid.g[d][i];
            }
            nextState.temperatures[i] = temp;
            nextState.TempAvg += nextState.temperatures[i];

            // Find Max and Min for the graph
            if (nextState.temperatures[i] > current_max) current_max = nextState.temperatures[i];
            if (nextState.temperatures[i] < current_min) current_min = nextState.temperatures[i];
        }
        nextState.TempAvg= nextState.TempAvg / cells;

        //update heatSource back it its oringinal temperature
        //doing it twice to ensure that the temperature reamins consitent and there is no flow
        if (previousState.isConstantHeatSource) {
            nextState.temperatures[previousState.heatSource] = MAX_TEMP;
            for (int d = 0; d < 9; ++d) {
                nextState.grid.g[d][previousState.heatSource] = weights[d] * previousState.temperatures[previousState.heatSource];
                nextState.grid.f[d][previousState.heatSource] = weights[d] *1.0; //a constant heat source should not have movement. It should radiate heat evenly
            }

            if (nextState.temperatures[nextState.heatSource] > current_max) {
                current_max = nextState.temperatures[nextState.heatSource];
            }
        }

        nextState.current_step = previousState.current_step + 1;
        {
            std::unique_lock<std::shared_mutex> lock(this->historyMutex);
            this->history->time_history.push_back(nextState.current_step);
            this->history->max_temp_history.push_back(current_max);
            this->history->min_temp_history.push_back(current_min);
            this->history->temperature_history.push_back(nextState.temperatures);
        }

        // Auto play check
        if (this->getAutoPlayStatus()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            this->stepFoward();
        }
    });
}

// Main Writer: Kristian
// Reviewer:
// Contributers:
void LocalEngine::stepBack() {
    thread->submitTask([this](const SimulationState& previousState, SimulationState& nextState) {
        if (previousState.current_step <= 0) return;

        nextState.current_step = previousState.current_step - 1;
        nextState.temperatures = this->history->temperature_history[nextState.current_step];
        nextState.grid = previousState.grid;
    });
}

// Main Writer: Kristian
// Reviewer:
// Contributers:
// Used by the timeline to change the simulation window (basically the same as stepback but goes to a particular step)
void LocalEngine::seekTo(int step) {
    thread->submitTask([this, step](const SimulationState& previousState, SimulationState& nextState) {
        if (step < 0 || step >= (int)this->history->temperature_history.size()) return;

        nextState.current_step = step;
        nextState.temperatures = this->history->temperature_history[nextState.current_step];
        nextState.grid = previousState.grid;
    });
}

// Main Writer: Gecenio
// Reviewer:
// Contributers:
double LocalEngine::getTotalEnergy() const {
    // Can't really make this run on a seperate thread without changing the function signature.
    auto state = thread->getState();
    return std::accumulate(state->temperatures.begin(), state->temperatures.end(), 0.0);
}


// Physics Functions (LBM)
// I will assume these functions are already running on the compute thread.
// Main Writer: Cosmin
// Reviewer:
// Contributers: Gecenio, Zeteny
void LocalEngine::Collision(double tauT, double TempAvg, double tauF, Grid& gridNew, const Grid &gridOld) {
    const double inv_tauF = 1.0 / tauF;
    const double inv_tauT = 1.0 / tauT;
    const double inv_cs2 = 1.0 / cs2;
    const double inv_cs4 = inv_cs2 * inv_cs2;
    const double half_inv_tauF = 1.0 - 0.5 / tauF;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = getIndex(x, y);

            std::array<double, 3> result = getDensityAndVelocity(gridOld, idx);
            double density = result[0];
            double ux = result[1];
            double uy = result[2];

            double temp = 0.0;
            for (int d = 0; d < 9; ++d) {
                temp += gridOld.g[d][idx];
            }

            double buoyancy = -lattice_buoyancy * (temp - TempAvg);
            double uyF = (density != 0.0) ? uy + 0.5 * buoyancy : 0.0;

            for (int d = 0; d < 9; ++d) {
                double cuF = cx[d] * ux + cy[d] * uyF;
                double cuT = cx[d] * ux + cy[d] * uyF;
                double cuF2 = cuF * cuF;
                double u2F = ux * ux + uyF * uyF;

                double feq = weights[d] * density *
                    (1.0 + cuF * inv_cs2 + cuF2 * 0.5 * inv_cs4 - u2F * 0.5 * inv_cs2);

                double forceTerm = weights[d] * half_inv_tauF *
                    (((cy[d] - uyF) * buoyancy) * inv_cs2 +
                     (cuF * (cy[d] * buoyancy)) * inv_cs4);

                gridNew.f[d][idx] = gridOld.f[d][idx] - inv_tauF * (gridOld.f[d][idx] - feq) + forceTerm;

                double geq = weights[d] * temp * (1.0 + cuT * inv_cs2);
                gridNew.g[d][idx] = gridOld.g[d][idx] - inv_tauT * (gridOld.g[d][idx] - geq);
            }
        }
    }
}

// Main Writer: Gecenio
// Reviewer: 
// Contributers: Cosmin, Zeteny
void LocalEngine::Stream(Grid &gridOld, Grid &gridNew) {
    for(int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Current cell 1D index
            int currentIndex = getIndex(x, y);

            // Streaming each direction
            // In SoA the main idea is to write
            // from the old grid current index
            // to the new grid neighbor index
            for (int d = 0; d < 9; ++d) {
                int sourceX = x - cx[d];
                int sourceY = y - cy[d];

                // check if the next x and y are in bound
                if (sourceX >= 0 && sourceY >= 0 && sourceX < width && sourceY < height) {
                    int sourceIndex = getIndex(sourceX, sourceY);
                    gridNew.g[d][currentIndex] = gridOld.g[d][sourceIndex];
                    gridNew.f[d][currentIndex] = gridOld.f[d][sourceIndex];
                }
                // if not in bound take the opposite direction (hits wall on the west, goes east instead)
                else {
                    int oppositeDir = inv[d];
                    gridNew.g[d][currentIndex] = gridOld.g[oppositeDir][currentIndex];
                    gridNew.f[d][currentIndex] = gridOld.f[oppositeDir][currentIndex];
                }
            }
        }
    }
}

// Main Writer: Cosmin
// Reviewer: 
// Contributers: 
std::array<double, 3> LocalEngine::getDensityAndVelocity(const Grid& gridOld,int idx){
    double density = 0.0;
        double ux=0.0;
        double uy=0.0;
        //we get density by adding all of the moving particles
        //the ux and uy represent the collection of the right moving particles and the left moving particles
        for (int d = 0; d < 9; ++d) {
            density +=gridOld.f[d][idx];
            ux=ux + (gridOld.f[d][idx]*cx[d]);
            uy=uy + (gridOld.f[d][idx]*cy[d]);
        }
        if (density!=0){
            ux/=density;
            uy/=density;
        }
    return {density, ux, uy};
}