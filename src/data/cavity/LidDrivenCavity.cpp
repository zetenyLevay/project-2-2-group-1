#include "LidDrivenCavity.h"
#include <cmath>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <numeric>

#ifdef _OPENMP
#include <omp.h>
#endif

// Main Writer: Gecenio
// Reviewer:
// Contributers:

LidDrivenCavity::LidDrivenCavity(int N, double Re, double U_lid)
    : SimulationEngine(N, N), N(N), Re_number(Re), lid_U(U_lid)
{
    // ν = U_lid * N / Re,  τ = 3ν + 0.5  (since cs² = 1/3)
    double nu = U_lid * N / Re;
    tau = 3.0 * nu + 0.5;

    if (tau <= 0.5) {
        std::cerr << "Warning: tau = " << tau << " (must be > 0.5 for stability). "
                  << "Try a smaller grid, higher Re, or lower U_lid.\n";
    }

    auto initialState = std::make_shared<SimulationState>();

    initialState->width = N;
    initialState->height = N;
    initialState->cells = N * N;

    initialState->grid = Grid(initialState->cells);
    initialState->current_step = 0;
    initialState->heat_spread = tau;
    initialState->viscosity = tau;
    initialState->TempAvg = 0.0;

    // Initialize with rho = 1.0, u = (0, 0) everywhere
    for (int i = 0; i < initialState->cells; ++i) {
        for (int d = 0; d < 9; ++d) {
            initialState->grid.f[d * initialState->cells + i] = weights[d] * 1.0;
            initialState->grid.g[d * initialState->cells + i] = 0.0;
        }
    }

    initialState->temperatures.resize(initialState->cells, 0.0); // velocity magnitude

    initialState->time_history.push_back(0);
    initialState->max_temp_history.push_back(0.0);  // max velocity
    initialState->min_temp_history.push_back(1.0);   // residual
    initialState->temperature_history.push_back(initialState->temperatures);
    initialState->grid_history.push_back(initialState->grid);

    gridTemp = Grid(initialState->cells);

    this->thread = std::make_unique<ReusableThread>(initialState);
}

void LidDrivenCavity::stepFoward() {
    thread->submitTask([this](SimulationState& state) {
        if (state.current_step < state.temperature_history.size() - 1) {
            state.current_step++;
            state.temperatures = state.temperature_history[state.current_step];
            if (!state.grid_history.empty() && state.current_step < state.grid_history.size()) {
                state.grid = state.grid_history[state.current_step];
            }
            return;
        }


        // BGK collision (no forcing and no thermal)
        Collision(tau, gridTemp, state.grid);

        // Streaming with the moving wall boundary condition
        Stream(gridTemp, state.grid, lid_U);

        // Compute velocity field and L2 residual of velocity magnitude
        const int n_cells = state.cells;
        const int w = state.width;
        const int h = state.height;

        double max_vel = 0.0;
        double sum_sq_diff = 0.0;
        double sum_sq_new = 0.0;

        #ifdef _OPENMP
        #pragma omp parallel for reduction(+:sum_sq_diff,sum_sq_new) reduction(max:max_vel)
        #endif
        for (int i = 0; i < n_cells; ++i) {
            double rho = 0.0;
            double ux = 0.0;
            double uy = 0.0;
            for (int d = 0; d < 9; ++d) {
                double val = state.grid.f[d * n_cells + i];
                rho += val;
                ux += val * cx[d];
                uy += val * cy[d];
            }
            if (rho != 0.0) {
                ux /= rho;
                uy /= rho;
            }

            double vel_new = std::sqrt(ux * ux + uy * uy);
            double vel_old = state.temperatures[i];
            double diff = vel_new - vel_old;

            sum_sq_diff += diff * diff;
            sum_sq_new += vel_new * vel_new;
            if (vel_new > max_vel) max_vel = vel_new;

            state.temperatures[i] = vel_new;
        }

        double residual = (sum_sq_new > 1e-10) ? std::sqrt(sum_sq_diff / sum_sq_new) : 0.0;

        state.current_step++;
        state.time_history.push_back(state.current_step);
        state.max_temp_history.push_back(max_vel);
        state.min_temp_history.push_back(residual);
        state.temperature_history.push_back(state.temperatures);
    });
}

void LidDrivenCavity::stepBack() {
    thread->submitTask([this](SimulationState& state) {
        if (state.current_step <= 0) return;
        state.current_step--;
        state.temperatures = state.temperature_history[state.current_step];
        if (!state.grid_history.empty() && state.current_step < state.grid_history.size()) {
            state.grid = state.grid_history[state.current_step];
        }
    });
}

void LidDrivenCavity::seekTo(int step) {
    thread->submitTask([this, step](SimulationState& state) {
        if (step < 0 || step >= (int)state.temperature_history.size()) return;
        state.current_step = step;
        state.temperatures = state.temperature_history[state.current_step];
        if (!state.grid_history.empty() && state.current_step < (int)state.grid_history.size()) {
            state.grid = state.grid_history[state.current_step];
        }
    });
}

// Main Writer: Gecenio
// Reviewer:
// Contributers:

// Isothermal BGK collision: no buoyancy, no forcing, no thermal g distribution.
void LidDrivenCavity::Collision(double tau, Grid& gridNew, const Grid& gridOld) {
    const int n_cells = cells;
    const int w = width;
    const int h = height;

    const double inv_tau = 1.0 / tau;
    const double inv_cs2 = 3.0;       // 1/cs² where cs² = 1/3
    const double inv_cs4 = 9.0;       // 1/cs⁴

#ifdef USE_OMP_TARGET_OFFLOAD
    const double* f_old = gridOld.f.data();
    double* f_new = gridNew.f.data();

    #pragma omp target teams distribute parallel for collapse(2) \
        map(to: f_old[:9*n_cells]) \
        map(from: f_new[:9*n_cells]) \
        firstprivate(n_cells, w, h, inv_tau, inv_cs2, inv_cs4)
#else
    const double* f_old = gridOld.f.data();
    double* f_new = gridNew.f.data();

    #ifdef _OPENMP
    #pragma omp parallel for collapse(2) schedule(dynamic)
    #endif
#endif
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;

            // Macroscopic moments from f
            double rho = 0.0;
            double ux = 0.0;
            double uy = 0.0;
            for (int d = 0; d < 9; ++d) {
                double val = f_old[d * n_cells + idx];
                rho += val;
                ux += val * cx[d];
                uy += val * cy[d];
            }
            if (rho != 0.0) {
                ux /= rho;
                uy /= rho;
            }

            double usq = ux * ux + uy * uy;

            // BGK collision
            for (int d = 0; d < 9; ++d) {
                double cu = cx[d] * ux + cy[d] * uy;

                // D2Q9 equilibrium
                double f_eq = weights[d] * rho * (
                    1.0 + cu * inv_cs2 + 0.5 * cu * cu * inv_cs4 - 0.5 * usq * inv_cs2
                );

                f_new[d * n_cells + idx] = f_old[d * n_cells + idx] - inv_tau * (f_old[d * n_cells + idx] - f_eq);
            }
        }
    }
}

// Main Writer: Gecenio
// Reviewer:
// Contributers:

// Streaming with bounce-back on stationary walls and
// Zou/He velocity BC on the top lid (moving wall).
void LidDrivenCavity::Stream(const Grid& gridOld, Grid& gridNew, double U_lid) {
    const int n_cells = cells;
    const int w = width;
    const int h = height;

    // Precompute lid correction coefficients
    // Top lid (y=0) moving right with velocity U_lid.
    // Unknown directions at the top wall going INTO the fluid: d=2, d=5, d=6.
    // Bounce-back with moving wall: f_new[d] = f_old[inv[d]] + 2*w_d*ρ*(e_d·u_wall)/cs²
    //   d=2 (0,1): normal, no y-velocity   -> correction = 0
    //   d=5 (1,1): e·u_wall = U_lid         -> correction = +U_lid/6
    //   d=6 (-1,1): e·u_wall = -U_lid        -> correction = -U_lid/6
    const double lid_corr_d5 =  U_lid / 6.0;
    const double lid_corr_d6 = -U_lid / 6.0;

#ifdef USE_OMP_TARGET_OFFLOAD
    const double* f_old = gridOld.f.data();
    double* f_new = gridNew.f.data();

    #pragma omp target teams distribute parallel for collapse(2) \
        map(to: f_old[:9*n_cells]) \
        map(from: f_new[:9*n_cells]) \
        firstprivate(n_cells, w, h, lid_corr_d5, lid_corr_d6)
#else
    const double* f_old = gridOld.f.data();
    double* f_new = gridNew.f.data();

    #ifdef _OPENMP
    #pragma omp parallel for collapse(2) schedule(static)
    #endif
#endif
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;
            bool is_top_wall = (y == 0);

            for (int d = 0; d < 9; ++d) {
                int sourceX = x - cx[d];
                int sourceY = y - cy[d];

                if (sourceX >= 0 && sourceY >= 0 && sourceX < w && sourceY < h) {
                    // Interior: normal streaming
                    int sourceIdx = sourceY * w + sourceX;
                    f_new[d * n_cells + idx] = f_old[d * n_cells + sourceIdx];

                } else if (is_top_wall && (d == 5 || d == 6)) {
                    // Top lid: moving wall (Zou/He velocity BC)
                    int opp = inv[d];
                    double base = f_old[opp * n_cells + idx];
                    f_new[d * n_cells + idx] = base + (d == 5 ? lid_corr_d5 : lid_corr_d6);

                } else {
                    // Stationary walls (bottom, left, right): regular bounce-back
                    int opp = inv[d];
                    f_new[d * n_cells + idx] = f_old[opp * n_cells + idx];
                }
            }
        }
    }
}

double LidDrivenCavity::getResidual() {
    auto state = getState();
    return state->min_temp_history.empty() ? 1.0 : state->min_temp_history.back();
}

bool LidDrivenCavity::hasConverged(double tolerance, int minSteps) {
    auto state = getState();
    if (state->current_step < minSteps) return false;
    if (state->min_temp_history.size() < 2) return false;

    // Check last few residuals for stable convergence
    int n = state->min_temp_history.size();
    for (int i = std::max(0, n - 10); i < n; ++i) {
        if (state->min_temp_history[i] > tolerance) return false;
    }
    return true;
}

void LidDrivenCavity::runUntilConvergence(double tolerance, int maxSteps, int minSteps) {
    std::cout << "Running lid-driven cavity: N=" << N
              << ", Re=" << Re_number
              << ", U_lid=" << lid_U
              << ", tau=" << tau << "\n";

    auto state = getMutableState();
    const int n = state->cells;

    for (int step = 0; step < maxSteps; ++step) {
        Collision(tau, gridTemp, state->grid);
        Stream(gridTemp, state->grid, lid_U);

        // Diagnostics on CPU — data is already on host after Stream's map(from: f_new...)
        const double* f_state = state->grid.f.data();
        double* temps = state->temperatures.data();

        double max_vel = 0.0;
        double sum_sq_diff = 0.0;
        double sum_sq_new = 0.0;

        #ifdef _OPENMP
        #pragma omp parallel for reduction(+:sum_sq_diff,sum_sq_new) reduction(max:max_vel)
        #endif
        for (int i = 0; i < n; ++i) {
            double rho = 0.0, ux = 0.0, uy = 0.0;
            for (int d = 0; d < 9; ++d) {
                double val = f_state[d * n + i];
                rho += val;
                ux += val * cx[d];
                uy += val * cy[d];
            }
            if (rho != 0.0) { ux /= rho; uy /= rho; }

            double vel_new = std::sqrt(ux * ux + uy * uy);
            double vel_old = temps[i];

            sum_sq_diff += (vel_new - vel_old) * (vel_new - vel_old);
            sum_sq_new += vel_new * vel_new;
            if (vel_new > max_vel) max_vel = vel_new;
            temps[i] = vel_new;
        }

        double residual = (sum_sq_new > 1e-10) ? std::sqrt(sum_sq_diff / sum_sq_new) : 0.0;

        if (step % 500 == 0 || step == maxSteps - 1) {
            std::cout << "  Step " << std::setw(6) << step
                      << " | residual: " << std::scientific << std::setprecision(6) << residual
                      << " | max |u|: " << std::fixed << std::setprecision(6) << max_vel << "\n";
        }

        // Update history so extractProfiles and getState see the results
        state->current_step = step;
        state->time_history.push_back(step);
        state->max_temp_history.push_back(max_vel);
        state->min_temp_history.push_back(residual);
        state->temperature_history.push_back(state->temperatures);

        if (step >= minSteps && residual < tolerance) {
            std::cout << "Converged at step " << step
                      << " (residual = " << std::scientific << residual << ")\n";
            break;
        }
    }
}

void LidDrivenCavity::extractProfiles(const std::string& filepath) {
    auto state = getState();

    std::filesystem::path pathObj(filepath);
    std::filesystem::path dir = pathObj.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }

    std::ofstream out(filepath);
    if (!out.is_open()) {
        std::cerr << "Failed to open " << filepath << " for writing profiles\n";
        return;
    }

    const int n = state->cells;
    const int w = state->width;
    const int h = state->height;
    const double* f = state->grid.f.data();

    out << "# Lid-driven cavity velocity profiles\n";
    out << "# Re = " << Re_number << ", N = " << N << ", U_lid = " << lid_U << "\n";
    out << "#\n";
    out << "# Vertical centerline (x = " << (w / 2) << "): u(y) at x = N/2\n";
    out << "# y/N, u/U_lid\n";

    int centerX = w / 2;
    int centerY = h / 2;

    for (int y = 0; y < h; ++y) {
        int idx = y * w + centerX;
        double rho = 0.0, ux = 0.0, uy = 0.0;
        for (int d = 0; d < 9; ++d) {
            double val = f[d * n + idx];
            rho += val;
            ux += val * cx[d];
            uy += val * cy[d];
        }
        if (rho != 0.0) {
            ux /= rho;
            uy /= rho;
        }
        double y_norm = (double)y / (h - 1);
        out << y_norm << ", " << (ux / lid_U) << "\n";
    }

    out << "#\n# Horizontal centerline (y = " << (h / 2) << "): v(x) at y = N/2\n";
    out << "# x/N, v/U_lid\n";

    for (int x = 0; x < w; ++x) {
        int idx = centerY * w + x;
        double rho = 0.0, ux = 0.0, uy = 0.0;
        for (int d = 0; d < 9; ++d) {
            double val = f[d * n + idx];
            rho += val;
            ux += val * cx[d];
            uy += val * cy[d];
        }
        if (rho != 0.0) {
            ux /= rho;
            uy /= rho;
        }
        double x_norm = (double)x / (w - 1);
        out << x_norm << ", " << (uy / lid_U) << "\n";
    }

    out << "#\n# Reynolds number: " << Re_number << "\n";
    out << "# Steps to convergence: " << state->current_step << "\n";

    out.close();
    std::cout << "Profiles saved to: " << filepath << "\n";
}

// CLI usage
void runCavityBenchmark(int N, double Re, double U_lid, double tolerance) {
    LidDrivenCavity cavity(N, Re, U_lid);

    cavity.runUntilConvergence(tolerance);

    std::string filepath = "../saves/cavity_Re" + std::to_string((int)Re)
                          + "_N" + std::to_string(N) + ".csv";
    cavity.extractProfiles(filepath);

    std::cout << "\nBenchmark complete. Compare " << filepath
              << " against Ghia et al. (1982) reference data.\n";
}
