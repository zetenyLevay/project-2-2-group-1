#include "LocalEngine.h"
#include "../../thread/ReusableThread.h"
#include <numeric>
#include <fstream>
#include <iostream>
#include <filesystem>

// OpenMP support
#ifdef _OPENMP
#include <omp.h>
#endif

// Initialized on main thread
// Main Writer: Berke/Kristian 
// Reviewer: 
// Contributers: 
LocalEngine::LocalEngine(int width, int height, bool constantHeatSource) : SimulationEngine(width, height) {
    auto initialState = std::make_unique<SimulationState>();

    // Initialize Basic Variables
    initialState->width = width;
    initialState->height = height;
    auto cells = initialState->cells = width * height;
    initialState->grid = Grid(cells);
    initialState->current_step = 0;
    initialState->isConstantHeatSource = constantHeatSource;

    // Initialize Physics
    initPhysics(*initialState);

    // Initialize Temperatures
    initialState->temperatures.resize(cells, ROOM_TEMP);

    // Initialize Grid (dont initialize in the physics function to avoid overwriting saves)
    double tempAvgLocal = 0.0;
    #ifdef _OPENMP
    #pragma omp parallel for reduction(+:tempAvgLocal)
    #endif
    for (int i = 0; i < cells; i++) {
        for (int d = 0; d < 9; ++d) {
            initialState->grid.g[d * cells + i] = weights[d] * initialState->temperatures[i];
            initialState->grid.f[d * cells + i] = weights[d] *1.0; //initializing the flow of the fluid
        }
        tempAvgLocal = tempAvgLocal + initialState->temperatures[i];
    }
    initialState->TempAvg = tempAvgLocal / cells;

    // Initialize history variables
    this->history = std::make_unique<SimulationHistory>();
    this->history->time_history.push_back(initialState->current_step);
    this->history->max_temp_history.push_back(ROOM_TEMP);
    this->history->min_temp_history.push_back(ROOM_TEMP);
    this->history->temperature_history.push_back(initialState->temperatures);
    this->history->convectionOutput.push_back(0.0);
    this->history->radiationOutput.push_back(0.0);

    // Launch the compute thread.
    this->thread = std::make_unique<ReusableThread>(std::move(initialState));
}

// Load simulation constructer 
// Main Writer: Berke 
// Reviewer: 
// Contributers: Kristian
LocalEngine::LocalEngine(int w, int h, bool constantHeatSource, std::unique_ptr<SimulationState> initialState, std::unique_ptr<SimulationHistory> initialHistory): SimulationEngine(w, h) {
    // Initialize physics
    initPhysics(*initialState);

    // New average
    double sum = 0.0;
    for (double t : initialState->temperatures) {
        sum += t;
    }
    initialState->TempAvg = sum / initialState->cells;
    
    this->history = std::move(initialHistory);
    this->thread = std::make_unique<ReusableThread>(std::move(initialState));
}

// Main Writer: Kristian 
// Reviewer: 
// Contributers: 
void LocalEngine::initPhysics(SimulationState& state) {
    // Initialize physics constants
    double cells_height = (double)height; 
    state.lattice_thermal_diffusivity = (cells_height*mach_number*cs)/(sqrt(rayliegh_num*prandtl_num));
    state.lattice_buoyancy = (mach_number*mach_number*cs2)/(delta_T*cells_height);
    state.lattice_kinematic_viscosity = prandtl_num*state.lattice_thermal_diffusivity;
    state.tauT = 3*state.lattice_thermal_diffusivity+0.5;
    state.tauF = 3*state.lattice_kinematic_viscosity+0.5;
    state.delta_x = room_height/cells_height;
    state.seconds_per_step = state.lattice_kinematic_viscosity*((state.delta_x*state.delta_x)/real_viscosity);
    state.lattice_stefan_boltzmann = radiator_emissivity * kinematic_sigma * (state.seconds_per_step / state.delta_x); // Lattice version of stefan-boltzmann constant

    state.heat_spread = state.tauT;
    state.viscosity = state.lattice_kinematic_viscosity;
    //relaxation times for heat_spread and visocsity
    //we are using 3 because we divide by cs2 which is 1/3
    //state.tauF = state.viscosity*3 +0.5;
    //state.tauT = state.heat_spread*3  +0.5;
    state.heatSourceW = width * 0.1;
    state.heatSourceH = height * 0.4;
    state.TempAvg = 0.0;

    // Set heatsources
    // (x,y) = (0,0); the radiator's position
    state.heatSources.clear();
    for (int y = radY; y < radY + state.heatSourceH && y < height; ++y) {
        for (int x = radX; x < radX + state.heatSourceW && x < width; ++x) {
            state.heatSources.push_back(getIndex(x,y));
        }
    }

    state.isRad.resize(cells, false);
    for (int idx : state.heatSources) {
        state.isRad[idx] = true;
    }

    std::vector<int> surfaceR;
    for (int idx : state.heatSources) {
        int rX = idx % width;
        int rY = idx / width;

        bool isSurface = ((rX == radX) || (rX == radX + state.heatSourceW -1) || (rY == radY) || (rY == radY + state.heatSourceH -1));

        if (isSurface) {
            surfaceR.push_back(idx);
        }
    }

    // Radiation Setup
    // Get boundary Cells
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (x == 0 || x == width - 1 || y == 0 || y == height - 1) {
                // Ignore radiator cells
                if (!state.isRad[getIndex(x,y)]) {
                    state.boundaryCells.push_back(getIndex(x,y));
                }
            }
        }
    }

    // Calculate View Factors (For each radiator cell, the distance from it to the wall)
    for (int radIdx : surfaceR) {
        int radX = radIdx % width;
        int radY = radIdx / width;

        double totalInverseDistance = 0.0;
        std::vector<double> temp(state.boundaryCells.size(), 0.0);

        for (int i = 0; i < state.boundaryCells.size(); i++) {
            int wallIdx = state.boundaryCells[i];
            int wallX = wallIdx % width;
            int wallY = wallIdx / width;

            // Distance
            double dist = std::sqrt(((radX - wallX) * (radX - wallX)) + ((radY - wallY) * (radY - wallY)));

            // Radiation intensity falls linearly with distance
            if (dist > 0) {
                temp[i] = 1.0 / dist;
                totalInverseDistance += temp[i];
            }
        }

        for (int i = 0; i < state.boundaryCells.size(); i++) {
            if (temp[i] > 0) {
                ViewFactor vf;
                vf.sourceIdx = radIdx;
                vf.targetIdx = state.boundaryCells[i];
                vf.factor = temp[i] / totalInverseDistance;
                state.viewFactors.push_back(vf);
            }
        }
    }

    int numThreads = 1;
    #ifdef _OPENMP
    numThreads = omp_get_max_threads();
    #endif
    state.localFluxCache.resize(numThreads, std::vector<double>(cells, 0.0));
    state.t4.resize(cells, 0.0);
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
        double current_max = ROOM_TEMP;
        double current_min = MAX_TEMP;

        // update heatSource back it its oringinal temperature and increment heat
        if (previousState.isConstantHeatSource) {
            for (int idx : previousState.heatSources) {
                nextState.temperatures[idx] = previousState.temperatures[idx];

                if(nextState.temperatures[idx] < MAX_TEMP){
                    nextState.temperatures[idx] += 0.003888; // Reaches 55 at around 9000 frames which is about 30 minutes
                }

                for (int d = 0; d < 9; ++d) {
                    nextState.grid.g[d* cells + idx] = weights[d] * nextState.temperatures[idx];
                    nextState.grid.f[d* cells + idx] = weights[d] * 1.0; //a constant heat source should not have movement. It should radiate heat evenly
                }

                if (nextState.temperatures[idx] > current_max) {
                    current_max = nextState.temperatures[idx];
                }
            }
        }

        double previousEnergy = previousState.TempAvg * cells;
        Grid gridTemp(previousState.cells);

        // Physics Steps
        this->Radiation(previousState, nextState, gridTemp);
        this->Collision(previousState.tauT, previousState.TempAvg, previousState.tauF, previousState.lattice_buoyancy, gridTemp, previousState.grid);
        this->Stream(gridTemp, nextState.grid, previousState.isRad, nextState);

        double tempAvgLocal = 0.0;
        #ifdef _OPENMP
        #pragma omp parallel for reduction(+:tempAvgLocal) \
            reduction(max:current_max) \
            reduction(min:current_min)
        #endif
        for (int i = 0; i < cells; i++) {
            double temp = 0.0;

            for (int d = 0; d < 9; ++d) {
                temp += nextState.grid.g[d * cells + i];
            }
            nextState.temperatures[i] = temp;
            tempAvgLocal = tempAvgLocal + nextState.temperatures[i];

            // Find Max and Min for the graph
            if (nextState.temperatures[i] > current_max) current_max = nextState.temperatures[i];
            if (nextState.temperatures[i] < current_min) current_min = nextState.temperatures[i];
        }
        nextState.TempAvg = tempAvgLocal / cells;

        double currentEnergy = nextState.TempAvg * cells;
        double totalEnergyOutput = currentEnergy - previousEnergy;
        // Get convection
        double convectionThisStep = totalEnergyOutput - history->radiationOutput.back();
        history->convectionOutput.push_back(convectionThisStep);

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
void LocalEngine::Collision(double tauT,double TempAvg,double tauF, double lattice_buoyancy, Grid& gridNew, const Grid &gridOld){
    const int n_cells = cells;
    const int w = width;
    const int h = height;

#ifdef USE_OMP_TARGET_OFFLOAD
    const double* g_old = gridOld.g.data();
    const double* f_old = gridOld.f.data();
    double* g_new = gridNew.g.data();
    double* f_new = gridNew.f.data();

    #pragma omp target teams distribute parallel for collapse(2) \
        map(to: g_old[:9*n_cells], f_old[:9*n_cells]) \
        map(from: g_new[:9*n_cells], f_new[:9*n_cells]) \
        firstprivate(n_cells, w, h, heat_spread, TempAvg, viscosity)
#else
    const double* g_old = gridOld.g.data();
    const double* f_old = gridOld.f.data();
    double* g_new = gridNew.g.data();
    double* f_new = gridNew.f.data();

    #ifdef _OPENMP
    #pragma omp parallel for collapse(2) schedule(dynamic)
    #endif
#endif
    for (int y = 0; y < h; y++){
        for(int x = 0; x < w; x++){
            int idx = y * w + x;

            std::array<double, 3> result = computeDensityAndVelocity(f_old, n_cells, idx);
            double density = result[0];
            double ux = result[1]; //horizontal velocity
            double uy = result[2]; //vertical velocity
            double temp = 0.0;
            for (int d = 0; d < 9; ++d) {
                temp += g_old[d * n_cells + idx];
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
            for (int d = 0; d < 9; ++d) {
                double cuF = cx[d]*ux + cy[d]*uyF;
                double forceTerm=weights[d] *(1.0- 0.5/tauF)*(((cy[d] -uyF) * buoyancy)/cs2 + ((cx[d]*ux + cy[d]*uyF)*(cy[d] * buoyancy))/(cs2 *cs2));
                f_new[d * n_cells + idx] = f_old[d * n_cells + idx] - (1.0/tauF) * (f_old[d * n_cells + idx] - weights[d] * density*(1 + cuF/cs2 + (cuF*cuF)/(2*cs2*cs2) -(ux*ux + uyF*uyF)/(2*cs2)))+forceTerm;

                double cuT=cx[d]*ux + cy[d]*uy;
                g_new[d * n_cells + idx] = g_old[d * n_cells + idx] - (1.0/tauT) * (g_old[d * n_cells + idx] - weights[d] * temp * (1+ cuT/cs2));
            }
        }
    }
}

// Main Writer: Gecenio
// Reviewer: 
// Contributers: Cosmin, Zeteny
void LocalEngine::Stream(Grid &gridOld, Grid &gridNew, const std::vector<bool>& isRad, SimulationState& state) {
    const int n_cells = cells;
    const int w = width;
    const int h = height;

#ifdef USE_OMP_TARGET_OFFLOAD
    double* g_old = gridOld.g.data();
    double* f_old = gridOld.f.data();
    double* g_new = gridNew.g.data();
    double* f_new = gridNew.f.data();

    #pragma omp target teams distribute parallel for collapse(2) \
        map(to: g_old[:9*n_cells], f_old[:9*n_cells]) \
        map(from: g_new[:9*n_cells], f_new[:9*n_cells]) \
        firstprivate(n_cells, w, h)
#else
    double* g_old = gridOld.g.data();
    double* f_old = gridOld.f.data();
    double* g_new = gridNew.g.data();
    double* f_new = gridNew.f.data();

    #ifdef _OPENMP
    #pragma omp parallel for collapse(2) schedule(static)
    #endif
#endif
    for(int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int currentIndex = y * w + x;

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
                if (sourceX >= 0 && sourceY >= 0 && sourceX < w && sourceY < h) {
                    int sourceIndex = sourceY * w + sourceX;

                    if (isRad[sourceIndex]) {
                        g_new[d * n_cells + currentIndex] = -g_old[oppositeDir * n_cells + sourceIndex] + 2.0 * weights[d] * state.temperatures[sourceIndex];
                        f_new[d * n_cells + currentIndex] = f_old[oppositeDir * n_cells + sourceIndex];
                    }
                    else {
                        g_new[d * n_cells + currentIndex] = g_old[d * n_cells + sourceIndex];
                        f_new[d * n_cells + currentIndex] = f_old[d * n_cells + sourceIndex];
                    }


                }
                // if not in bound take the opposite direction (hits wall on the west, goes east instead)
                else {

                    g_new[d * n_cells + currentIndex] = g_old[oppositeDir * n_cells + currentIndex];
                    f_new[d * n_cells + currentIndex] = f_old[oppositeDir * n_cells + currentIndex];
                }
            }
        }
    }
}

// Main Writer: Kristian
// Reviewer:
// Contributers:
void LocalEngine::Radiation(const SimulationState& previousState, SimulationState& nextState, Grid& targetGrid) {
    double radiationThisStep = 0.0;

    int numThreads = 1;
    #ifdef _OPENMP  
    numThreads = omp_get_max_threads();
    #endif

    nextState.localFluxCache.resize(numThreads, std::vector<double>(previousState.cells, 0.0));
    nextState.t4.resize(previousState.cells, 0.0);

    #pragma omp parallel for
    for (int i = 0; i < previousState.cells; ++i) {
        double tk = previousState.temperatures[i] + 273.15;
        double tk2 = tk * tk;
        nextState.t4[i] = tk2 * tk2;
    }

    #pragma omp parallel
    {
        int tid = 0;
        #ifdef _OPENMP
        tid = omp_get_thread_num();
        #endif

        std::fill(nextState.localFluxCache[tid].begin(), nextState.localFluxCache[tid].end(), 0.0);

        double localRad = 0;

        #pragma omp for schedule(dynamic)
        for (int i = 0; i < previousState.viewFactors.size(); ++i) {
            const auto& vf = previousState.viewFactors[i];

            // Stefan-Boltzmann law
            double heatFlux = nextState.lattice_stefan_boltzmann * vf.factor * (nextState.t4[vf.sourceIdx] - nextState.t4[vf.targetIdx]);

            // Accounting for walls
            double wallHeatCapacity = 50.0;
            heatFlux = heatFlux / wallHeatCapacity;

            localRad += heatFlux;

            nextState.localFluxCache[tid][vf.targetIdx] += heatFlux;
        }

        #pragma omp atomic
        radiationThisStep += localRad;
    }

    #pragma omp parallel for
    for (int i = 0; i < previousState.boundaryCells.size(); ++i) {
        int idx = previousState.boundaryCells[i];

        double totalFlux = 0.0;
        for (int t = 0; t < numThreads; ++t) {
            totalFlux += nextState.localFluxCache[t][idx];
        }

        if (totalFlux != 0.0) {
            for (int d = 0; d < 9; ++d) {
                targetGrid.g[d*previousState.cells + idx] += weights[d] * totalFlux;
            }
        }
    }
    // Helper to tune radiation
    history->radiationOutput.push_back(radiationThisStep);
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
            density +=gridOld.f[d * gridOld.cells + idx];
            ux=ux + (gridOld.f[d * gridOld.cells + idx]*cx[d]);
            uy=uy + (gridOld.f[d * gridOld.cells + idx]*cy[d]);
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

    state->width = w;
    state->height = h;
    state->cells = w * h;
    state->isConstantHeatSource = constantHeat;
    state->grid = Grid(state->cells);

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

    // Get convection and radiation history
    history->convectionOutput.resize(history_count, 0.0);
    history->radiationOutput.resize(history_count, 0.0);
    in.read(reinterpret_cast<char*>(history->convectionOutput.data()), history_count * sizeof(double));
    in.read(reinterpret_cast<char*>(history->radiationOutput.data()), history_count * sizeof(double));

    // Get most recent heat grid
    for (int d = 0; d < 9; ++d) {
        in.read(reinterpret_cast<char*>(state->grid.g.data() + d * state->cells), state->cells * sizeof(double));
    }

    // Get most recent fluid grid
    for (int d = 0; d < 9; ++d) {
        in.read(reinterpret_cast<char*>(state->grid.f.data() + d * state->cells), state->cells * sizeof(double));
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

    // Write radiation and convection history
    out.write(reinterpret_cast<const char*>(history->convectionOutput.data()), history_count * sizeof(double));
    out.write(reinterpret_cast<const char*>(history->radiationOutput.data()), history_count * sizeof(double));

    // Write most recent heat grid
    for (int d = 0; d < 9; ++d) {
        out.write(reinterpret_cast<const char*>(state.grid.g.data() + d * state.cells), state.cells * sizeof(double));
    }

    // Write most recent fluid grid
    for (int d = 0; d < 9; ++d) {
        out.write(reinterpret_cast<const char*>(state.grid.f.data() + d * state.cells), state.cells * sizeof(double));
    }

    out.close();
    return true;
}