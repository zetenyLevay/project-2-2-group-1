#include "main.h"
using namespace std;

int getIndex(int x, int y) {
    int wrappedX = (x + WIDTH) % WIDTH;
    int wrappedY = (y + HEIGHT) % HEIGHT;
    return wrappedY * WIDTH + wrappedX;
}

void ThermalCollision(const Grid& grid, Grid& newGrid, double heat_spread){
    for (int y = 0; y < HEIGHT; y++){
        for(int x = 0; x < WIDTH; x++){
            int idx= getIndex(x,y);

            // Calculating temperature of every g inside a cell
            double temp = 0.0;
            for (int d = 0; d < 9; ++d) {
                temp += grid.g[d][idx];
            }
            //getting the density and the velocity from the Fluid Lattice Boltzmann calculations
            array<double, 3> result = getDensityAndVelocity(grid, idx);
            double density = result[0];
            double ux = result[1];
            double uy = result[2];

            // Calculating the equilibrium function for every g inside of a cell and applying the collision to a new grid
            for (int d = 0; d < 9; ++d) {
                double cu=cx[d]*ux + cy[d]*uy;
                newGrid.g[d][idx] = grid.g[d][idx] - (1.0/heat_spread) * (grid.g[d][idx] - weights[d] *density * temp * (1+ cu/cs2));
            }
        }
    }
}
void FluidCollision(const Grid& grid, Grid& newGrid, double viscosity){
     for (int y = 0; y < HEIGHT; y++){
        for(int x = 0; x < WIDTH; x++){
            int idx= getIndex(x,y);

            // Calculating density of every f inside a cell
            array<double, 3> result = getDensityAndVelocity(grid, idx);
            double density = result[0];
            double ux = result[1];
            double uy = result[2];
            
            
            // Calculating the equilibrium function for every f inside of a cell and applying the collision to a new grid
            for (int d = 0; d < 9; ++d) {
                double cu=cx[d]*ux + cy[d]*uy;
                newGrid.f[d][idx] = grid.f[d][idx] - (1.0/viscosity) * (grid.f[d][idx] - weights[d] * density*(1 + cu/cs2 + (cu*cu)/(2*cs2*cs2) -(ux*ux + uy*uy)/(2*cs2)));
            }
        }
    }
}

void stream(const Grid& gridOld, Grid& gridNew) {
    for(int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            // Current cell 1D index
            int currentIndex = getIndex(x, y);

            // Streaming each direction
            // In SoA the main idea is to write
            // from the old grid current index
            // to the new grid neighbor index
            for (int d = 0; d < 9; ++d) {
                int nindex = getIndex(x + cx[d], y + cy[d]);
                gridNew.g[d][nindex] = gridOld.g[d][currentIndex];
                gridNew.f[d][nindex] = gridOld.f[d][currentIndex];
            }
        }
    }
}
array<double, 3> getDensityAndVelocity(const Grid& grid,int idx){
    double density = 0.0;
        double ux=0.0;
        double uy=0.0;
        for (int d = 0; d < 9; ++d) {
            density +=grid.f[d][idx];
            ux=ux + (grid.f[d][idx]*cx[d]);
            uy=uy + (grid.f[d][idx]*cy[d]);
        }
        if (density>0){
            ux/=density;
            uy/=density;
        }else{
            ux=0.0;
            uy=0.0;
        }
    return {density, ux, uy};
}
