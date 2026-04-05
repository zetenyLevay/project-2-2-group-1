#include "main.h"

int getIndex(int x, int y) {
    int wrappedX = (x + WIDTH) % WIDTH;
    int wrappedY = (y + HEIGHT) % HEIGHT;
    return wrappedY * WIDTH + wrappedX;
}
void FluidCollision(const Grid& grid, Grid& newGrid, double viscosity,double TempAvg){
     for (int y = 0; y < HEIGHT; y++){
        for(int x = 0; x < WIDTH; x++){
            int idx= getIndex(x,y);

            // Calculating density of every f inside a cell
            std::array<double, 3> result = getDensityAndVelocity(grid, idx);
            double density = result[0];
            double ux = result[1];
            double uy = result[2];
            double temp = 0.0;
            for (int d = 0; d < 9; ++d) {
                temp += grid.g[d][idx]; // sum all directions to get macroscopic temperature
            }
            
            double buoyancy = density * 0.01 *(temp-TempAvg)*-1e-5;  //-1e-5 represents the donward gravity
            //we use half force to better represent how and when the force is applied, the second half will be added from the forceTerm
            double uyF=0.0; //half force term of uy
            // because the buoyancy value of ux is 0 we do not need to calculate the half force term of ux, we can just use ux
            if(density!=0){
                uyF=uy+  0.5 * buoyancy / density;
            }

            // Calculating the equilibrium function for every f inside of a cell and applying the collision to a new grid
            for (int d = 0; d < 9; ++d) {
                double cu = cx[d]*ux + cy[d]*uyF;
                //Guo Forcing term
                double forceTerm=weights[d] *(1.0- 0.5/viscosity)*((cy[d] * buoyancy- uyF * buoyancy)/cs2 + (cu*(cy[d] * buoyancy))/(cs2 *cs2));
                newGrid.f[d][idx] = grid.f[d][idx] - (1.0/viscosity) * (grid.f[d][idx] - weights[d] * density*(1 + cu/cs2 + (cu*cu)/(2*cs2*cs2) -(ux*ux + uyF*uyF)/(2*cs2)))+forceTerm;
            }
        }
    }
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
            std::array<double, 3> result = getDensityAndVelocity(grid, idx);
            double density = result[0];
            double ux = result[1];
            double uy = result[2];
           
            // Calculating the equilibrium function for every g inside of a cell and applying the collision to a new grid
            for (int d = 0; d < 9; ++d) {
                double cu=cx[d]*ux + cy[d]*uy;
                newGrid.g[d][idx] = grid.g[d][idx] - (1.0/heat_spread) * (grid.g[d][idx] - weights[d] * temp * (1+ cu/cs2));
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

std::array<double, 3> getDensityAndVelocity(const Grid& grid,int idx){
    double density = 0.0;
        double ux=0.0;
        double uy=0.0;
        for (int d = 0; d < 9; ++d) {
            density +=grid.f[d][idx];
            ux=ux + (grid.f[d][idx]*cx[d]);
            uy=uy + (grid.f[d][idx]*cy[d]);
        }
        if (density!=0){
            ux/=density;
            uy/=density;
        }
    return {density, ux, uy};
}
