#include "main.h"

int getIndex(int x, int y) {
    int wrappedX = (x + WIDTH) % WIDTH;
    int wrappedY = (y + HEIGHT) % HEIGHT;
    return wrappedY * WIDTH + wrappedX;
}
void CollisionStep(const Grid& grid, Grid& newGrid, double viscosity,double TempAvg,double heat_spread){
     for (int y = 0; y < HEIGHT; y++){
        for(int x = 0; x < WIDTH; x++){
            int idx= getIndex(x,y);

            // Calculating density of every f inside a cell
            std::array<double, 3> result = getDensityAndVelocity(grid, idx);
            double density = result[0];
            double ux = result[1]; //horizontal velocity
            double uy = result[2]; //vertical velocity
            double temp = 0.0;
            //calculating the temperature for every direction inside a cell
            for (int d = 0; d < 9; ++d) {
                temp += grid.g[d][idx]; 
            }
            //buoyancy is calculated using a simplied version of the Boussinesq approximation: beta * (T-Tavg)
            //buoyancy represents how much the hot fluid wants to rise up
            double buoyancy = 2*1e-5 *(temp-TempAvg);  //2*1e-5 represents the thermal expansion strenght

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
                newGrid.f[d][idx] = grid.f[d][idx] - (1.0/viscosity) * (grid.f[d][idx] - weights[d] * density*(1 + cuF/cs2 + (cuF*cuF)/(2*cs2*cs2) -(ux*ux + uyF*uyF)/(2*cs2)))+forceTerm;
                //The complete Lattice boltzmann Thermal formula
                double cuT=cx[d]*ux + cy[d]*uy;
                newGrid.g[d][idx] = grid.g[d][idx] - (1.0/heat_spread) * (grid.g[d][idx] - weights[d] * temp * (1+ cuT/cs2));
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
                int sourceX = x - cx[d];
                int sourceY = y - cy[d];

                // check if the next x and y are in bound
                if (sourceX >= 0 && sourceY >= 0 && sourceX < WIDTH && sourceY < HEIGHT) {
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
