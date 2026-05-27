#include "LocalEngine.h"
#include "../../thread/ReusableThread.h"
#include <numeric>
#include <fstream>
#include <iostream>
#include <filesystem>

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
        this->history->time_history.push_back(nextState.current_step);
        this->history->max_temp_history.push_back(current_max);
        this->history->min_temp_history.push_back(current_min);
        this->history->temperature_history.push_back(nextState.temperatures);

        // Auto play check
        if (this->getAutoPlayStatus()) {
            this->stepFoward();
        }
    });
}

// Main Writer: Kristian
// Reviewer: 
// Contributers:
void LocalEngine::stepBack() {
    thread->submitTask([this](const SimulationState& previousState, SimulationState& nextState) {
        // Prevent going back beyond initial state
        if (previousState.current_step <= 0) return;
    
        // Decrement the current step
        nextState.current_step = previousState.current_step - 1;

        nextState.temperatures = history->temperature_history[nextState.current_step];
    });
}

// Main Writer: Kristian
// Reviewer:
// Contributers:
// Used by the timeline to change the simulation window (basically the same as stepback but goes to a particular step)
void LocalEngine::seekTo(int step) {
    thread->submitTask([this, step](const SimulationState& previousState, SimulationState& nextState) {
        // Prevent going out of bounds
        if (step < 0 || step >= history->temperature_history.size()) return;

        nextState.current_step = step;
        nextState.temperatures = history->temperature_history[nextState.current_step];
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
void LocalEngine::Collision(double tauT,double TempAvg,double tauF, Grid& gridNew, const Grid &gridOld){

    //gridNew - output grid, gridOld - input grid
    //heat_spred - controls temperature update
	// tempAvg - average temperature of the grid
	// viscosity - controls the fluid movement update


    for (int y = 0; y < height; y++){
        for(int x = 0; x < width; x++){
            int idx= getIndex(x,y);

            // Calculating density of every f inside a cell
            std::array<double, 3> result = getDensityAndVelocity(gridOld, idx);
            double density = result[0];
            double ux = result[1]; //horizontal velocity
            double uy = result[2]; //vertical velocity
            double temp = 0.0;
            //calculating the temperature for every direction inside a cell
            for (int d = 0; d < 9; ++d) {
                temp += gridOld.g[d][idx]; 
            }
            //buoyancy is calculated using a simplied version of the Boussinesq approximation: beta * (T-Tavg)
            //buoyancy represents how much the hot fluid wants to rise up
            double buoyancy = -lattice_buoyancy *(temp-ROOM_TEMP);  //4*1e-5 represents the thermal expansion strenght

            //we use half force to better represent how and when the force is applied, the second half will be added from the forceTerm
            // because the buoyancy value of ux is 0 we do not need to calculate the half force term of ux, we can just use ux
            //half force term of uy
            double uyF=0.0; 
            if(density!=0){
                uyF=uy+  0.5 * buoyancy / density;
            }

            // Calculating the equilibrium function for every f inside of a cell and applying the collision to a new grid
            for (int d = 0; d < 9; ++d) {
                double cuF = cx[d]*ux + cy[d]*uyF;
                //Guo Forcing term. Used to correctly add force(adding movement due to the heat) to the collision step of the Lattice Boltzmann method
                double forceTerm=weights[d] *(1.0- 0.5/density_relaxation_time)*(((cy[d] -uyF) * buoyancy)/cs2 + ((cx[d]*ux + cy[d]*uyF)*(cy[d] * buoyancy))/(cs2 *cs2));
                //The complete Lattice Boltzmann Fluid movement formula
                gridNew.f[d][idx] = gridOld.f[d][idx] - (1.0/density_relaxation_time) * (gridOld.f[d][idx] - weights[d] * density*(1 + cuF/cs2 + (cuF*cuF)/(2*cs2*cs2) -(ux*ux + uyF*uyF)/(2*cs2)))+forceTerm;
                //The complete Lattice boltzmann Thermal formula
                double cuT=cx[d]*ux + cy[d]*uyF;
                gridNew.g[d][idx] = gridOld.g[d][idx] - (1.0/thermal_relaxation_time) * (gridOld.g[d][idx] - weights[d] * temp * (1+ cuT/cs2));
                
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

// Main Writer: Kristian
// Reviewer: 
// Contributers: 
std::unique_ptr<LocalEngine> loadLocalSimulation(const std::string& filepath) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return nullptr;
    }

    // Width and Height
    int w, h;
    in.read(reinterpret_cast<char*>(&w), sizeof(w));
    in.read(reinterpret_cast<char*>(&h), sizeof(h));

    bool constantHeat;
    in.read(reinterpret_cast<char*>(&constantHeat), sizeof(constantHeat));

    std::unique_ptr<SimulationState> state = std::make_unique<SimulationState>();
    std::unique_ptr<SimulationHistory> history = std::make_unique<SimulationHistory>();

    // Get history length
    size_t history_count;
    in.read(reinterpret_cast<char*>(&history_count), sizeof(history_count));

    // Read basic history information
    history->time_history.resize(history_count);
    history->max_temp_history.resize(history_count);
    history->min_temp_history.resize(history_count);

    in.read(reinterpret_cast<char*>(history->time_history.data()), history_count * sizeof(double));
    in.read(reinterpret_cast<char*>(history->max_temp_history.data()), history_count * sizeof(double));
    in.read(reinterpret_cast<char*>(history->min_temp_history.data()), history_count * sizeof(double));

    // Get full temperature history
    history->temperature_history.resize(history_count, std::vector<double>(state->cells));
    for (size_t i = 0; i < history_count; ++i) {
        in.read(reinterpret_cast<char*>(history->temperature_history[i].data()), state->cells * sizeof(double));
    }


    // Write most recent grid
    for (int d = 0; d < 9; ++d) {
        in.read(reinterpret_cast<char*>(state->grid.g[d].data()), state->cells * sizeof(double));
    }

    // Go to the last frame of the sim
    if (history_count > 0) {
        state->current_step = history->time_history.back();
        state->temperatures = history->temperature_history.back();
    }

    in.close();
    return std::make_unique<LocalEngine>(w, h, constantHeat, std::move(state), std::move(history));
}

// Main Writer: Kristian
// Reviewer: 
// Contributers: 
bool saveSimulation(const SimulationState& state, const SimulationHistory* history, const std::string& filepath) {
    std::filesystem::path pathObj(filepath);
    std::filesystem::path dir = pathObj.parent_path();

    if (!dir.empty() && !std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
        std::cout << "Created missing saves folder" << std::endl;
    }

    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) return false;

    // Write width and height
    out.write(reinterpret_cast<const char*>(&state.width), sizeof(state.width));
    out.write(reinterpret_cast<const char*>(&state.height), sizeof(state.height));

    // Write whether the heat is constant
    out.write(reinterpret_cast<const char*>(&state.isConstantHeatSource), sizeof(state.isConstantHeatSource));

    // Write history length
    size_t history_count = history->time_history.size();
    out.write(reinterpret_cast<const char*>(&history_count), sizeof(history_count));

    // Write basic history information
    out.write(reinterpret_cast<const char*>(history->time_history.data()), history_count * sizeof(double));
    out.write(reinterpret_cast<const char*>(history->max_temp_history.data()), history_count * sizeof(double));
    out.write(reinterpret_cast<const char*>(history->min_temp_history.data()), history_count * sizeof(double));

    // Write temperature history
    for (size_t i = 0; i < history_count; ++i) {
        out.write(reinterpret_cast<const char*>(history->temperature_history[i].data()), state.cells * sizeof(double));
    }

    // Get most recent grid
    for (int d = 0; d < 9; ++d) {
        out.write(reinterpret_cast<const char*>(state.grid.g[d].data()), state.cells * sizeof(double));
    }

    out.close();
    return true;
}