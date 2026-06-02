#if CUDA_AVAILABLE == 1

#include "./LocalCUDAEngine.cuh"
#include <numeric>

// D2Q9 lattice constants in constant memory
// Those are cached and broadcasted to all threads

__constant__ double d_cx[9];
__constant__ double d_cy[9];
__constant__ double d_weights[9];
__constant__ double d_cs2;
__constant__ int    d_inv[9];

bool cuda_initialized = false;

void initCudaLattice() {
    // Make sure the constant variables are only initialized once.
    // Not sure if necessary.
    if (cuda_initialized) return;
    cuda_initialized = true;

    const double h_cx[9]     = {0.0, 1.0, 0.0, -1.0, 0.0,  1.0, -1.0, -1.0,  1.0};
    const double h_cy[9]     = {0.0, 0.0, 1.0,  0.0, -1.0, 1.0,  1.0, -1.0, -1.0};
    const double h_weights[9]= {4.0/9.0,
                                1.0/9.0, 1.0/9.0, 1.0/9.0, 1.0/9.0,
                                1.0/36.0, 1.0/36.0, 1.0/36.0, 1.0/36.0};
    const int    h_inv[9]     = {0, 3, 4, 1, 2, 7, 8, 5, 6};
    double h_cs2 = 1.0 / 3.0;

    cudaMemcpyToSymbol(d_cx,      h_cx,      9 * sizeof(double));
    cudaMemcpyToSymbol(d_cy,      h_cy,      9 * sizeof(double));
    cudaMemcpyToSymbol(d_weights, h_weights, 9 * sizeof(double));
    cudaMemcpyToSymbol(d_inv,     h_inv,     9 * sizeof(int));
    cudaMemcpyToSymbol(d_cs2,     &h_cs2,    sizeof(double));
}

void LocalCUDAEngine::initializeCuda() {
    initCudaLattice();
    
    size_t bytes = this->n_vals * sizeof(double);
    cudaMalloc(&this->g_src, bytes);
    cudaMalloc(&this->f_src, bytes);
    cudaMalloc(&this->g_mid, bytes);
    cudaMalloc(&this->f_mid, bytes);
    cudaMalloc(&this->g_dst, bytes);
    cudaMalloc(&this->f_dst, bytes);
}

LocalCUDAEngine::LocalCUDAEngine(int w, int h, bool constantHeatSource) : SimulationEngine(w, h), n_vals(9 * cells) {
    this->initializeCuda();

    auto initialState = std::make_unique<SimulationState>();

    initialState->width = w;
    initialState->height = h;

    auto cells = initialState->cells = w * h;

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

LocalCUDAEngine::LocalCUDAEngine(int w, int h, bool constantHeatSource, std::unique_ptr<SimulationState> initialState, std::unique_ptr<SimulationHistory> initialHistory): SimulationEngine(w, h), n_vals(9 * cells) {
    this->initializeCuda();

    this->history = std::move(initialHistory);
    this->thread = std::make_unique<ReusableThread>(std::move(initialState));
}

LocalCUDAEngine::~LocalCUDAEngine() {
    cudaFree(this->g_src); cudaFree(this->f_src);
    cudaFree(this->g_mid); cudaFree(this->f_mid);
    cudaFree(this->g_dst); cudaFree(this->f_dst);
}

void LocalCUDAEngine::pack(const Grid& grid) {
    for (int d = 0; d < 9; ++d) {
        cudaMemcpy(this->g_src + d * this->cells, grid.g[d].data(), this->cells * sizeof(double), cudaMemcpyHostToDevice);
        cudaMemcpy(this->f_src + d * this->cells, grid.f[d].data(), this->cells * sizeof(double), cudaMemcpyHostToDevice);
    }
}

// Device helpers
__device__ void getDensityAndVelocity(const double* f_old, int idx, int n_cells,
                                      double& density, double& ux, double& uy) {
    density = 0.0;
    ux = 0.0;
    uy = 0.0;
    for (int d = 0; d < 9; ++d) {
        double val = f_old[d * n_cells + idx];
        density += val;
        ux += val * d_cx[d];
        uy += val * d_cy[d];
    }
    if (density != 0.0) {
        ux /= density;
        uy /= density;
    }
}

__global__ void collisionKernel(int height, int width,
                                double tauT, double TempAvg, double tauF,
                                double* g_new, double* f_new,
                                const double* g_old, const double* f_old) {

    int x = blockDim.x * blockIdx.x + threadIdx.x;
    int y = blockDim.y * blockIdx.y + threadIdx.y;

    if (x >= width || y >= height) return;

    int n_cells = width * height;
    int idx = y * width + x;

    double inv_tauF = 1.0 / tauF;
    double inv_tauT = 1.0 / tauT;
    double inv_cs2 = 1.0 / d_cs2;
    double inv_cs4 = inv_cs2 * inv_cs2;
    double half_inv_tauF = 1.0 - 0.5 / tauF;

    double density, ux, uy;
    getDensityAndVelocity(f_old, idx, n_cells, density, ux, uy);

    double temp = 0.0;
    for (int d = 0; d < 9; ++d) {
        temp += g_old[d * n_cells + idx];
    }

    double buoyancy = 4.0e-5 * (temp - TempAvg);
    double uyF = (density != 0.0) ? uy + 0.5 * buoyancy : 0.0;

    for (int d = 0; d < 9; ++d) {
        int mem = d * n_cells + idx;

        double cuF  = d_cx[d] * ux  + d_cy[d] * uyF;
        double cuT  = d_cx[d] * ux  + d_cy[d] * uy;
        double cuF2 = cuF * cuF;
        double u2F  = ux * ux + uyF * uyF;

        double feq = d_weights[d] * density *
            (1.0 + cuF * inv_cs2 + cuF2 * 0.5 * inv_cs4 - u2F * 0.5 * inv_cs2);

        double forceTerm = d_weights[d] * half_inv_tauF *
            (((d_cy[d] - uy) * buoyancy) * inv_cs2 +
             (cuF * (d_cy[d] * buoyancy)) * inv_cs4);

        f_new[mem] = f_old[mem] - inv_tauF * (f_old[mem] - feq) + forceTerm;

        double geq = d_weights[d] * temp * (1.0 + cuT * inv_cs2);
        g_new[mem] = g_old[mem] - inv_tauT * (g_old[mem] - geq);
    }
}

__global__ void streamKernel(int height, int width,
                             double* g_new, double* f_new,
                             const double* g_old, const double* f_old) {

    int x = blockDim.x * blockIdx.x + threadIdx.x;
    int y = blockDim.y * blockIdx.y + threadIdx.y;

    if (x >= width || y >= height) return;

    int n_cells = width * height;
    int idx = y * width + x;

    for (int d = 0; d < 9; ++d) {
        int srcX = x - (int)d_cx[d];
        int srcY = y - (int)d_cy[d];
        int mem = d * n_cells + idx;

        if (srcX >= 0 && srcY >= 0 && srcX < width && srcY < height) {
            int srcIdx = srcY * width + srcX;
            int srcMem = d * n_cells + srcIdx;
            g_new[mem] = g_old[srcMem];
            f_new[mem] = f_old[srcMem];
        } else {
            int opp = d_inv[d];
            int oppMem = opp * n_cells + idx;
            g_new[mem] = g_old[oppMem];
            f_new[mem] = f_old[oppMem];
        }
    }
}


void LocalCUDAEngine::collision(const double tauT, const double TempAvg, const double tauF) {
    dim3 block(16, 16);
    dim3 grid((this->width + block.x - 1) / block.x,
              (this->height + block.y - 1) / block.y);

    collisionKernel<<<grid, block>>>(this->height, this->width,
                                     tauT, TempAvg, tauF,
                                     this->g_mid, this->f_mid,
                                     this->g_src, this->f_src);

    cudaError_t err = cudaDeviceSynchronize();

    if (err != cudaSuccess) {
        fprintf(stderr, "collisionKernel error: %s\n", cudaGetErrorString(err));
        exit(1);
    }
}

void LocalCUDAEngine::stream() {
    dim3 block(16, 16);
    dim3 grid((this->width + block.x - 1) / block.x,
              (this->height + block.y - 1) / block.y);

    streamKernel<<<grid, block>>>(this->height, this->width,
                                  this->g_dst, this->f_dst,
                                  this->g_mid, this->f_mid);

    cudaError_t err = cudaDeviceSynchronize();

    if (err != cudaSuccess) {
        fprintf(stderr, "streamKernel error: %s\n", cudaGetErrorString(err));
        exit(1);
    }
}

void LocalCUDAEngine::unpack(Grid& grid) const {
    for (int d = 0; d < 9; ++d) {
        cudaMemcpy(grid.g[d].data(), g_dst + d * cells, cells * sizeof(double), cudaMemcpyDeviceToHost);
        cudaMemcpy(grid.f[d].data(), f_dst + d * cells, cells * sizeof(double), cudaMemcpyDeviceToHost);
    }
}

void LocalCUDAEngine::stepFoward() {
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

        this->pack(previousState.grid);
        this->collision(previousState.tauT, previousState.TempAvg, previousState.tauF);
        this->stream();
        this->unpack(nextState.grid);

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
void LocalCUDAEngine::stepBack() {
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
void LocalCUDAEngine::seekTo(int step) {
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
double LocalCUDAEngine::getTotalEnergy() const {
    // Can't really make this run on a seperate thread without changing the function signature.
    auto state = thread->getState();
    return std::accumulate(state->temperatures.begin(), state->temperatures.end(), 0.0);
}

#endif