#include "LocalEngine.h"
#include <numeric>
#include <fstream>
#include <iostream>
#include <filesystem>

// Initialized on main thread
// Main Writer: Berke/Kristian 
// Reviewer: 
// Contributers: 
LocalEngine::LocalEngine(int width, int height, bool constantHeatSource) : SimulationEngine(width, height) {
    auto initialState = std::make_shared<SimulationState>();

    initialState->width = width;
    initialState->height = height;

    auto cells = initialState->cells = width * height;

    initialState->grid = initialState->cells;

    initialState->current_step = 0;
    initialState->heat_spread = thermal_relaxation_time;
    initialState->viscosity = lattice_kinematic_viscosity;
    initialState->TempAvg=0.0;
    initialState->heatSourceW = width * 0.1;
    initialState->heatSourceH = height * 0.4;
    
    // Set heatsources
    // (x,y) = (1,0); the radiator's position
    for (int y = radY; y < radY + initialState->heatSourceH && y < height; ++y) {
        for (int x = radX; x < radX + initialState->heatSourceW && x < width; ++x) {

            initialState->heatSources.push_back(getIndex(x,y));
        }
    }

    //relaxation times for heat_spread and visocsity
    //we are using 3 because we divide by cs2 which is 1/3
    initialState->tauF = initialState->viscosity*3 +0.5;
    initialState->tauT = initialState->heat_spread*3  +0.5;
    initialState->TempAvg = 0.0;
    initialState->isConstantHeatSource = constantHeatSource;
    initialState->temperatures.resize(cells, ROOM_TEMP);
    initialState->isRad.resize(cells, false);

    // Set heatsource for frame 0
    for (int idx : initialState->heatSources) {
        initialState->temperatures[idx] = MAX_TEMP;
        initialState->isRad[idx] = true;
    }

    // Initialize Grid 
    for (int i = 0; i < cells; i++) {
        for (int d = 0; d < 9; ++d) {
            initialState->grid.g[d][i] = weights[d] * initialState->temperatures[i];
            initialState->grid.f[d][i] = weights[d] *1.0; //initializing the flow of the fluid 
        }
        initialState->TempAvg = initialState->TempAvg + initialState->temperatures[i];
    }
    initialState->TempAvg = initialState->TempAvg/cells;

    history.time_history.push_back(initialState->current_step);
    history.max_temp_history.push_back(MAX_TEMP);
    history.min_temp_history.push_back(ROOM_TEMP);
    history.temperature_history.push_back(initialState->temperatures);
    history.convectionOutput.push_back(0.0);
    history.radiationOutput.push_back(0.0);

    // Radiation Setup
    // Get boundary Cells
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (x == 0 || x == width - 1 || y == 0 || y == height - 1) {
                // Ignore radiator cells
                if (!initialState->isRad[getIndex(x,y)]) {
                    initialState->boundaryCells.push_back(getIndex(x,y));
                }
            }
        }
    }

    // Calculate View Factors (For each radiator cell, the distance from it to the wall)
    for (int radIdx : initialState->heatSources) {
        int radX = radIdx % width;
        int radY = radIdx / width;

        double totalInverseDistance = 0.0;
        std::vector<double> temp(initialState->boundaryCells.size(), 0.0);

        for (int i = 0; i < initialState->boundaryCells.size(); i++) {
            int wallIdx = initialState->boundaryCells[i];
            int wallX = wallIdx % width;
            int wallY = wallIdx / width;

            // Distance
            double dist = std::sqrt(std::pow(radX - wallX, 2) + std::pow(radY - wallY, 2));

            // Radiation intensity falls linearly with distance
            if (dist > 0) {
                temp[i] = 1.0 / dist;
                totalInverseDistance += temp[i];
            }
        } 

        for (int i = 0; i < initialState->boundaryCells.size(); i++) {
            if (temp[i] > 0) {
                ViewFactor vf;
                vf.sourceIdx = radIdx;
                vf.targetIdx = initialState->boundaryCells[i];
                vf.factor = temp[i] / totalInverseDistance;
                initialState->viewFactors.push_back(vf);
            }
        }
    }

    // Launch the compute thread.
    this->thread = std::make_unique<ReusableThread>(initialState);
}

// Main Writer: Gecenio 
// Reviewer: 
// Contributers: Kristian, Berke
void LocalEngine::stepFoward() {
    thread->submitTask([this](SimulationState& state) {
        // If already calculated just set the grid and temperatures again 
        if (state.current_step < history.temperature_history.size() - 1) {
            state.current_step++;
            state.temperatures = history.temperature_history[state.current_step];

            // Auto play check
            if (this->getAutoPlayStatus()) {
                this->stepFoward();
            }

            return;
        }

        // update heatSource back it its oringinal temperature
        if (state.isConstantHeatSource) {
            for (int idx : state.heatSources) {
                state.temperatures[idx] = MAX_TEMP;
                for (int d = 0; d < 9; ++d) {
                    state.grid.g[d][idx] = weights[d] * state.temperatures[idx];
                    state.grid.f[d][idx] = weights[d] * 1.0; //a constant heat source should not have movement. It should radiate heat evenly
                }
            }
        }

        double previousEnergy = state.TempAvg * cells;

        Grid gridTemp(state.cells);

        // Physics steps
        this->Radiation(state);
        this->Collision(state.tauT,state.TempAvg,state.tauF, gridTemp, state.grid);
        this->Stream(gridTemp, state.grid, state.isRad);

        double current_max = ROOM_TEMP;
        double current_min = MAX_TEMP;
        //update heatSource back it its oringinal temperature
        //doing it twice to ensure that the temperature reamins consitent and there is no flow
        if (state.isConstantHeatSource) {
            for (int idx : state.heatSources) {
                state.temperatures[idx] = MAX_TEMP;
                for (int d = 0; d < 9; ++d) {
                    state.grid.g[d][idx] = weights[d] * state.temperatures[idx];
                    state.grid.f[d][idx] = weights[d] *1.0; //a constant heat source should not have movement. It should radiate heat evenly
                }

                if (state.temperatures[idx] > current_max) {
                    current_max = state.temperatures[idx];
                }
            }
        }

        // Get average temperature
        state.TempAvg = 0.0;
        for (int i = 0; i < cells; i++) {
            double temp = 0.0;

            for (int d = 0; d < 9; ++d) {
                temp += state.grid.g[d][i];
            }
            state.temperatures[i] = temp;
            state.TempAvg = state.TempAvg + state.temperatures[i];

            // Find Max and Min for the graph
            if (state.temperatures[i] > current_max) current_max = state.temperatures[i];
            if (state.temperatures[i] < current_min) current_min = state.temperatures[i];
        }
        state.TempAvg = state.TempAvg / cells;

        // Get the total output
        double currentEnergy = state.TempAvg * cells;
        double totalEnergyOutput = currentEnergy - previousEnergy;

        // Get convection
        double convectionThisStep = totalEnergyOutput - history.radiationOutput.back();
        history.convectionOutput.push_back(convectionThisStep);

        state.current_step++;
        history.time_history.push_back(state.current_step);
        history.max_temp_history.push_back(current_max);
        history.min_temp_history.push_back(current_min);
        history.temperature_history.push_back(state.temperatures);

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
    thread->submitTask([this](SimulationState& state) {
        // Prevent going back beyond initial state
        if (state.current_step <= 0) return;
    
        // Decrement the current step
        state.current_step--;

        state.temperatures = history.temperature_history[state.current_step];
    });
}

// Main Writer: Kristian
// Reviewer:
// Contributers:
// Used by the timeline to change the simulation window (basically the same as stepback but goes to a particular step)
void LocalEngine::seekTo(int step) {
    thread->submitTask([this, step](SimulationState& state) {
        // Prevent going out of bounds
        if (step < 0 || step >= history.temperature_history.size()) return;

        state.current_step = step;
        state.temperatures = history.temperature_history[state.current_step];
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
void LocalEngine::Collision(double tauT,double TempAvg,double tauF, Grid& gridNew, Grid &gridOld){
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
            double buoyancy = lattice_buoyancy * (temp-ROOM_TEMP);  //4*1e-5 represents the thermal expansion strenght

            //we use half force to better represent how and when the force is applied, the second half will be added from the forceTerm
            // because the buoyancy value of ux is 0 we do not need to calculate the half force term of ux, we can just use ux
            //half force term of uy
            double uyF=0.0; 
            if(density!=0){
                uyF=uy+  0.5 * buoyancy / density;
            }

            // Calculating the equilibrium function for every f inside of a cell and applying the collision to a new grid
            // CHANGED BACK TO tauF and tauT SOMEONE CHECK THIS
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
void LocalEngine::Stream(Grid &gridOld, Grid &gridNew, std::vector<bool>& isRad) {
    for(int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Current cell 1D index
            int currentIndex = getIndex(x, y);
            
            if (isRad[currentIndex]) continue;

            // Streaming each direction
            // In SoA the main idea is to write
            // from the old grid current index
            // to the new grid neighbor index
            for (int d = 0; d < 9; ++d) {
                int sourceX = x - cx[d];
                int sourceY = y - cy[d];
                int oppositeDir = inv[d];

                // check if the next x and y are in bound
                if (sourceX >= 0 && sourceY >= 0 && sourceX < width && sourceY < height) {
                    int sourceIndex = getIndex(sourceX, sourceY);

                    if (isRad[sourceIndex]) {
                        // It hit the radiator
                        gridNew.f[d][currentIndex] = gridOld.f[oppositeDir][currentIndex];
                        gridNew.g[d][currentIndex] = -gridOld.g[oppositeDir][currentIndex] + 2.0 * weights[d] * MAX_TEMP;
                    }
                    else {
                        // Normal flow
                        gridNew.g[d][currentIndex] = gridOld.g[d][sourceIndex];
                        gridNew.f[d][currentIndex] = gridOld.f[d][sourceIndex];
                    }
                }
                // if not in bound take the opposite direction (hits wall on the west, goes east instead)
                else {
                    
                    gridNew.g[d][currentIndex] = gridOld.g[oppositeDir][currentIndex];
                    gridNew.f[d][currentIndex] = gridOld.f[oppositeDir][currentIndex];
                }
            }
        }
    }
}

// Main Writer: Kristian
// Reviewer: 
// Contributers: 
void LocalEngine::Radiation(SimulationState& state) {
    double radiationThisStep = 0.0;
    for (const auto& vf : state.viewFactors) {
        double tSource = state.temperatures[vf.sourceIdx];
        double tTarget = state.temperatures[vf.targetIdx];

        // Covert to kelvin
        double tSourceK = tSource + 273.15;
        double tTargetK = tTarget + 273.15;

        // Stefan-Boltzmann law 
        double heatFlux = lattice_stefan_boltzmann * vf.factor * (std::pow(tSourceK, 4) - std::pow(tTargetK, 4));

        // Accounting for walls
        double wallHeatCapacity = 50.0;
        heatFlux = heatFlux / wallHeatCapacity;

        radiationThisStep += heatFlux;

        // Add the heat
        for (int i = 0; i < 9; ++i) {
            state.grid.g[i][vf.targetIdx] += weights[i] * heatFlux;
        }
    }
    // Helper to tune radiation 
    history.radiationOutput.push_back(radiationThisStep);
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

    // Create Engine
    auto loadedEngine = std::make_unique<LocalEngine>(w, h, constantHeat);
    auto state = loadedEngine->getMutableState();

    // Get history length
    size_t history_count;
    in.read(reinterpret_cast<char*>(&history_count), sizeof(history_count));

    // Read basic history information
    loadedEngine->history.time_history.resize(history_count);
    loadedEngine->history.max_temp_history.resize(history_count);
    loadedEngine->history.min_temp_history.resize(history_count);

    in.read(reinterpret_cast<char*>(loadedEngine->history.time_history.data()), history_count * sizeof(double));
    in.read(reinterpret_cast<char*>(loadedEngine->history.max_temp_history.data()), history_count * sizeof(double));
    in.read(reinterpret_cast<char*>(loadedEngine->history.min_temp_history.data()), history_count * sizeof(double));

    // Get full temperature history
    loadedEngine->history.temperature_history.resize(history_count, std::vector<double>(state->cells));
    for (size_t i = 0; i < history_count; ++i) {
        in.read(reinterpret_cast<char*>(loadedEngine->history.temperature_history[i].data()), state->cells * sizeof(double));
    }


    // Write most recent grid
    for (int d = 0; d < 9; ++d) {
        in.read(reinterpret_cast<char*>(state->grid.g[d].data()), state->cells * sizeof(double));
    }

    // Go to the last frame of the sim
    if (history_count > 0) {
        state->current_step = loadedEngine->history.time_history.back();
        state->temperatures = loadedEngine->history.temperature_history.back();
    }

    in.close();
    return loadedEngine;
}

// Main Writer: Kristian
// Reviewer: 
// Contributers: 
bool saveSimulation(const SimulationState& state, const SimulationHistory& history, const std::string& filepath) {
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
    size_t history_count = history.time_history.size();
    out.write(reinterpret_cast<const char*>(&history_count), sizeof(history_count));

    // Write basic history information
    out.write(reinterpret_cast<const char*>(history.time_history.data()), history_count * sizeof(double));
    out.write(reinterpret_cast<const char*>(history.max_temp_history.data()), history_count * sizeof(double));
    out.write(reinterpret_cast<const char*>(history.min_temp_history.data()), history_count * sizeof(double));

    // Write temperature history
    for (size_t i = 0; i < history_count; ++i) {
        out.write(reinterpret_cast<const char*>(history.temperature_history[i].data()), state.cells * sizeof(double));
    }

    // Get most recent grid
    for (int d = 0; d < 9; ++d) {
        out.write(reinterpret_cast<const char*>(state.grid.g[d].data()), state.cells * sizeof(double));
    }

    out.close();
    return true;
}