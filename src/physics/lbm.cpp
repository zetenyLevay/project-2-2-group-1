#include "main.h"

int getIndex(int x, int y) {
    int wrappedX = (x + WIDTH) % WIDTH;
    int wrappedY = (y + HEIGHT) % HEIGHT;
    return wrappedY * WIDTH + wrappedX;
}

void Collision(const Grid& grid, Grid& newGrid, double heat_spread){
    double temp = 0.0;

    for (int y = 0; y < HEIGHT; y++){
        for(int x = 0; x < WIDTH; x++){
            int idx= getIndex(x,y);

            // Calculating temperature of every g inside a cell
            double temp = 0.0;
            for (int d = 0; d < 9; ++d) {
                temp += grid.g[d][idx];
            }

            // Calculating the equilibrium function for every g inside of a cell and applying the collision to a new grid
            for (int d = 0; d < 9; ++d) {
                newGrid.g[d][idx] = grid.g[d][idx] - (1.0/heat_spread) * (grid.g[d][idx] - weights[d] * temp);
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
            }
        }
    }
}