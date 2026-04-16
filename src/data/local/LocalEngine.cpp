#include "LocalEngine.h"
#include <numeric>
#include <fstream>
#include <iostream>
#include <filesystem>

// Initialized on main thread
// Main Writer: Berke/Kristian 
// Reviewer: 
// Contributers: 
LocalEngine::LocalEngine(int width, int height) : SimulationEngine(width, height) {
    auto initialState = std::make_shared<SimulationState>();

    initialState->width = width;
    initialState->height = height;

    auto cells = initialState->cells = width * height;

    initialState->grid = initialState->cells;

    initialState->current_step = 0;
    initialState->heat_spread = 0.8;
    initialState->viscosity = 0.6;
    initialState->TempAvg=0.0;
    initialState->temperatures.resize(cells, 20.0); // room temp assumption
    initialState->temperatures[getIndex(0,0)] = MAX_TEMP; // Set heat source 

    // Initialize Grid 
    for (int i = 0; i < cells; i++) {
        for (int d = 0; d < 9; ++d) {
            initialState->grid.g[d][i] = weights[d] * initialState->temperatures[i];
            initialState->grid.f[d][i] = weights[d] *1.0; //initializing the flow of the fluid 
        }
        initialState->TempAvg = initialState->TempAvg + initialState->temperatures[i];
    }
    initialState->TempAvg = initialState->TempAvg/cells;

    initialState->time_history.push_back(initialState->current_step);
    initialState->max_temp_history.push_back(MAX_TEMP);
    initialState->min_temp_history.push_back(ROOM_TEMP);
    initialState->temperature_history.push_back(initialState->temperatures);

    // Launch the compute thread.
    this->thread = std::make_unique<ReusableThread>(initialState);
}

// Main Writer: Gecenio 
// Reviewer: 
// Contributers: Kristian, Berke
// Mutates: grid, temperatures, current_step, time_history, max_temp_history, min_temp_history, temperature_history, grid_history
void LocalEngine::stepFoward() {
    thread->submitTask([this](SimulationState& state) {
        // If already calculated just set the grid and temperatures again 
        if (state.current_step < state.temperature_history.size() - 1) {
            state.current_step++;
            state.temperatures = state.temperature_history[state.current_step];

            return;
        }

        Grid gridTemp(state.cells);
        this->Collision(state.heat_spread,state.TempAvg,state.viscosity, gridTemp, state.grid);

        this->Stream(gridTemp, state.grid);

        double current_max = ROOM_TEMP;
        double current_min = MAX_TEMP;
        state.TempAvg=0.0;
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
        state.TempAvg= state.TempAvg / cells;


        state.current_step++;
        state.time_history.push_back(state.current_step);
        state.max_temp_history.push_back(current_max);
        state.min_temp_history.push_back(current_min);
        state.temperature_history.push_back(state.temperatures);
    });
}

// Main Writer: Kristian
// Reviewer: 
// Contributers:
// Mutates: current_step, time_history, max_temp_history, min_temp_history, temperature_history, grid_history, temperatures, grid
void LocalEngine::stepBack() {
    thread->submitTask([this](SimulationState& state) {
        // Prevent going back beyond initial state
        if (state.current_step <= 0) return;
    
        // Decrement the current step
        state.current_step--;

        state.temperatures = state.temperature_history[state.current_step];
    });
}

// Main Writer: Kristian
// Reviewer: 
// Contributers:
// Used by the timeline to change the simulation window (basically the same as stepback but goes to a particular step)
void LocalEngine::seekTo(int step) {
    thread->submitTask([this, step](SimulationState& state) {
        // Prevent going out of bounds
        if (step < 0 || step >= state.temperature_history.size()) return;

        state.current_step = step;
        state.temperatures = state.temperature_history[state.current_step];
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
void LocalEngine::Collision(double heat_spread,double TempAvg,double viscosity, Grid& gridNew, Grid &gridOld){
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
            double buoyancy = 4*1e-5 *(temp-TempAvg);  //4*1e-5 represents the thermal expansion strenght

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
                double forceTerm=weights[d] *(1.0- 0.5/viscosity)*((cy[d] * buoyancy)/cs2 + ((cx[d]*ux + cy[d]*uy)*(cy[d] * buoyancy))/(cs2 *cs2));
                //The complete Lattice Boltzmann Fluid movement formula
                gridNew.f[d][idx] = gridOld.f[d][idx] - (1.0/viscosity) * (gridOld.f[d][idx] - weights[d] * density*(1 + cuF/cs2 + (cuF*cuF)/(2*cs2*cs2) -(ux*ux + uyF*uyF)/(2*cs2)))+forceTerm;
                //The complete Lattice boltzmann Thermal formula
                double cuT=cx[d]*ux + cy[d]*uy;
                gridNew.g[d][idx] = gridOld.g[d][idx] - (1.0/heat_spread) * (gridOld.g[d][idx] - weights[d] * temp * (1+ cuT/cs2));
                
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

    // Savetype
    SaveType saveType;
    in.read(reinterpret_cast<char*>(&saveType), sizeof(saveType));

    // Width and Height
    int w, h;
    in.read(reinterpret_cast<char*>(&w), sizeof(w));
    in.read(reinterpret_cast<char*>(&h), sizeof(h));

    // Create Engine
    auto loadedEngine = std::make_unique<LocalEngine>(w, h);
    auto state = loadedEngine->getMutableState();

    // Get history length
    size_t history_count;
    in.read(reinterpret_cast<char*>(&history_count), sizeof(history_count));

    // Read basic history information
    state->time_history.resize(history_count);
    state->max_temp_history.resize(history_count);
    state->min_temp_history.resize(history_count);

    in.read(reinterpret_cast<char*>(state->time_history.data()), history_count * sizeof(double));
    in.read(reinterpret_cast<char*>(state->max_temp_history.data()), history_count * sizeof(double));
    in.read(reinterpret_cast<char*>(state->min_temp_history.data()), history_count * sizeof(double));

    // Get full temperature history
    state->temperature_history.resize(history_count, std::vector<double>(state->cells));
    for (size_t i = 0; i < history_count; ++i) {
        in.read(reinterpret_cast<char*>(state->temperature_history[i].data()), state->cells * sizeof(double));
    }


    // Write most recent grid
    for (int d = 0; d < 9; ++d) {
        in.read(reinterpret_cast<char*>(state->grid.g[d].data()), state->cells * sizeof(double));
    }

    // Go to the last frame of the sim
    if (history_count > 0) {
        state->current_step = state->time_history.back();
        state->temperatures = state->temperature_history.back();
    }

    in.close();
    return loadedEngine;
}

// Main Writer: Kristian
// Reviewer: 
// Contributers: 
bool saveSimulation(const SimulationState state, const std::string& filepath, const SaveType saveType) {
    std::filesystem::path pathObj(filepath);
    std::filesystem::path dir = pathObj.parent_path();

    if (!dir.empty() && !std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
        std::cout << "Created missing saves folder" << std::endl;
    }

    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) return false;

    // Write the type of save
    out.write(reinterpret_cast<const char*>(&saveType), sizeof(saveType));

    // Write width and height
    out.write(reinterpret_cast<const char*>(&state.width), sizeof(state.width));
    out.write(reinterpret_cast<const char*>(&state.height), sizeof(state.height));

    // Write history length
    size_t history_count = state.time_history.size();
    out.write(reinterpret_cast<const char*>(&history_count), sizeof(history_count));

    // Write basic history information
    out.write(reinterpret_cast<const char*>(state.time_history.data()), history_count * sizeof(double));
    out.write(reinterpret_cast<const char*>(state.max_temp_history.data()), history_count * sizeof(double));
    out.write(reinterpret_cast<const char*>(state.min_temp_history.data()), history_count * sizeof(double));

    // Write temperature history
    for (size_t i = 0; i < history_count; ++i) {
        out.write(reinterpret_cast<const char*>(state.temperature_history[i].data()), state.cells * sizeof(double));
    }

    // Get most recent grid
    for (int d = 0; d < 9; ++d) {
        out.write(reinterpret_cast<const char*>(state.grid.g[d].data()), state.cells * sizeof(double));
    }

    out.close();
    return true;
}