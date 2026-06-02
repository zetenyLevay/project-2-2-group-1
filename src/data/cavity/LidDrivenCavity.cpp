#include "LidDrivenCavity.h"
#include "../../thread/ReusableThread.h"
#include <cmath>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <thread>

#ifdef _OPENMP
#include <omp.h>
#endif

LidDrivenCavity::LidDrivenCavity(int N, double Re, double U_lid)
    : SimulationEngine(N, N), N(N), Re_number(Re), lid_U(U_lid)
{
    double nu = U_lid * N / Re;
    tau = 3.0 * nu + 0.5;

    if (tau <= 0.5) {
        std::cerr << "Warning: tau = " << tau << " (must be > 0.5 for stability). "
                  << "Try a smaller grid, higher Re, or lower U_lid.\n";
    }

    auto initialState = std::make_unique<SimulationState>();

    initialState->width = N;
    initialState->height = N;
    initialState->cells = N * N;

    initialState->grid = Grid(initialState->cells);
    initialState->current_step = 0;
    initialState->heat_spread = tau;
    initialState->viscosity = tau;
    initialState->TempAvg = 0.0;

    for (int i = 0; i < initialState->cells; ++i) {
        for (int d = 0; d < 9; ++d) {
            initialState->grid.f[d * initialState->cells + i] = weights[d] * 1.0;
            initialState->grid.g[d * initialState->cells + i] = 0.0;
        }
    }

    initialState->temperatures.resize(initialState->cells, 0.0);

    gridTemp = Grid(initialState->cells);

    this->history = std::make_unique<SimulationHistory>();
    this->history->time_history.push_back(0);
    this->history->max_temp_history.push_back(0.0);
    this->history->min_temp_history.push_back(1.0);
    this->history->temperature_history.push_back(initialState->temperatures);

    this->thread = std::make_unique<ReusableThread>(std::move(initialState));
}

void LidDrivenCavity::stepFoward() {
    thread->submitTask([this](const SimulationState& previousState, SimulationState& nextState) {
        if (previousState.current_step < (int)this->history->temperature_history.size() - 1) {
            nextState.current_step = previousState.current_step + 1;
            nextState.temperatures = this->history->temperature_history[nextState.current_step];
            return;
        }

        Grid gridTemp(previousState.cells);

        Collision(tau, gridTemp, previousState.grid);
        Stream(gridTemp, nextState.grid, lid_U);

        const int n_cells = previousState.cells;
        const int w = previousState.width;
        const int h = previousState.height;

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
                double val = nextState.grid.f[d * n_cells + i];
                rho += val;
                ux += val * cx[d];
                uy += val * cy[d];
            }
            if (rho != 0.0) {
                ux /= rho;
                uy /= rho;
            }

            double vel_new = std::sqrt(ux * ux + uy * uy);
            double vel_old = previousState.temperatures[i];
            double diff = vel_new - vel_old;

            sum_sq_diff += diff * diff;
            sum_sq_new += vel_new * vel_new;
            if (vel_new > max_vel) max_vel = vel_new;

            nextState.temperatures[i] = vel_new;
        }

        double residual = (sum_sq_new > 1e-10) ? std::sqrt(sum_sq_diff / sum_sq_new) : 0.0;

        nextState.current_step = previousState.current_step + 1;
        this->history->time_history.push_back(nextState.current_step);
        this->history->max_temp_history.push_back(max_vel);
        this->history->min_temp_history.push_back(residual);
        this->history->temperature_history.push_back(nextState.temperatures);
    });
}

void LidDrivenCavity::stepBack() {
    thread->submitTask([this](const SimulationState& previousState, SimulationState& nextState) {
        if (previousState.current_step <= 0) return;
        nextState.current_step = previousState.current_step - 1;
        nextState.temperatures = this->history->temperature_history[nextState.current_step];
    });
}

void LidDrivenCavity::seekTo(int step) {
    thread->submitTask([this, step](const SimulationState& previousState, SimulationState& nextState) {
        if (step < 0 || step >= (int)this->history->temperature_history.size()) return;
        nextState.current_step = step;
        nextState.temperatures = this->history->temperature_history[step];
    });
}

// Isothermal BGK collision: no buoyancy, no forcing, no thermal g distribution.
void LidDrivenCavity::Collision(double tau, Grid& gridNew, const Grid& gridOld) {
    const int n_cells = cells;
    const int w = width;
    const int h = height;

    const double* f_old = gridOld.f.data();
    double* f_new = gridNew.f.data();

    const double inv_tau = 1.0 / tau;
    const double inv_cs2 = 3.0;
    const double inv_cs4 = 9.0;

    #ifdef _OPENMP
    #pragma omp parallel for collapse(2) schedule(dynamic)
    #endif
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;

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

            for (int d = 0; d < 9; ++d) {
                double cu = cx[d] * ux + cy[d] * uy;
                double f_eq = weights[d] * rho * (
                    1.0 + cu * inv_cs2 + 0.5 * cu * cu * inv_cs4 - 0.5 * usq * inv_cs2
                );
                f_new[d * n_cells + idx] = f_old[d * n_cells + idx] - inv_tau * (f_old[d * n_cells + idx] - f_eq);
            }
        }
    }
}

// Streaming with bounce-back on stationary walls and Zou/He velocity BC on the top lid.
void LidDrivenCavity::Stream(const Grid& gridOld, Grid& gridNew, double U_lid) {
    const int n_cells = cells;
    const int w = width;
    const int h = height;

    const double* f_old = gridOld.f.data();
    double* f_new = gridNew.f.data();

    const double lid_corr_d5 =  U_lid / 6.0;
    const double lid_corr_d6 = -U_lid / 6.0;

    #ifdef _OPENMP
    #pragma omp parallel for collapse(2) schedule(static)
    #endif
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;
            bool is_top_wall = (y == 0);

            for (int d = 0; d < 9; ++d) {
                int sourceX = x - cx[d];
                int sourceY = y - cy[d];

                if (sourceX >= 0 && sourceY >= 0 && sourceX < w && sourceY < h) {
                    int sourceIdx = sourceY * w + sourceX;
                    f_new[d * n_cells + idx] = f_old[d * n_cells + sourceIdx];
                } else if (is_top_wall && (d == 5 || d == 6)) {
                    int opp = inv[d];
                    double base = f_old[opp * n_cells + idx];
                    f_new[d * n_cells + idx] = base + (d == 5 ? lid_corr_d5 : lid_corr_d6);
                } else {
                    int opp = inv[d];
                    f_new[d * n_cells + idx] = f_old[opp * n_cells + idx];
                }
            }
        }
    }
}

double LidDrivenCavity::getResidual() {
    return this->history->min_temp_history.empty() ? 1.0 : this->history->min_temp_history.back();
}

bool LidDrivenCavity::hasConverged(double tolerance, int minSteps) {
    const auto* h = this->history.get();
    if ((int)h->time_history.size() - 1 < minSteps) return false;
    if (h->min_temp_history.size() < 2) return false;

    int n = h->min_temp_history.size();
    for (int i = std::max(0, n - 10); i < n; ++i) {
        if (h->min_temp_history[i] > tolerance) return false;
    }
    return true;
}

void LidDrivenCavity::runUntilConvergence(double tolerance, int maxSteps, int minSteps) {
    std::cout << "Running lid-driven cavity: N=" << N
              << ", Re=" << Re_number
              << ", U_lid=" << lid_U
              << ", tau=" << tau << "\n";

    for (int step = 0; step < maxSteps; ++step) {
        stepFoward();

        auto state = getState();
        int synced = 0;
        while (state->current_step <= step) {
            state = getState();
            std::this_thread::yield();
            if (++synced > 1000000) break;
        }

        double res = getResidual();

        if (step % 500 == 0 || step == maxSteps - 1) {
            std::cout << "  Step " << std::setw(6) << state->current_step
                      << " | residual: " << std::scientific << std::setprecision(6) << res
                      << " | max |u|: " << std::fixed << std::setprecision(6) << this->history->max_temp_history.back()
                      << "\n";
        }

        if (step >= minSteps && hasConverged(tolerance, minSteps)) {
            std::cout << "Converged at step " << state->current_step
                      << " (residual = " << std::scientific << res << ")\n";
            return;
        }
    }

    std::cout << "Reached maxSteps=" << maxSteps
              << " with residual=" << std::scientific << getResidual() << "\n";
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

void runCavityBenchmark(int N, double Re, double U_lid, double tolerance) {
    LidDrivenCavity cavity(N, Re, U_lid);
    cavity.runUntilConvergence(tolerance);

    std::string filepath = "../saves/cavity_Re" + std::to_string((int)Re)
                          + "_N" + std::to_string(N) + ".csv";
    cavity.extractProfiles(filepath);

    std::cout << "\nBenchmark complete. Compare " << filepath
              << " against Ghia et al. (1982) reference data.\n";
}
