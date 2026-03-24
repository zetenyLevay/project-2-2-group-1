#include "main.h"

int getIndex(int x, int y) {
    int wrappedX = (x + WIDTH) % WIDTH;
    int wrappedY = (y + HEIGHT) % HEIGHT;
    return wrappedY * WIDTH + wrappedX;
}
void Collision(const Grid& grid, Grid& newGrid,dobule heat_spread){
    double temp=0;
    Grid newGrid;
    for (int y=0;y<height; y++){
        for(int x=0; x<width; x++){
            int idx= getIndex(x,y);
            //calculating temperature of every g inside a cell
            temp=grid.g0[idx]+grid.g1[idx]+grid.g2[idx]+
                 grid.g3[idx]+grid.g4[idx]+grid.g5[idx]+
                 grid.g6[idx]+grid.g7[idx]+grid.g8[idx];
            

            //calculating the equilibrium function for every g inside of a cell and applying the collision to a new grid
                newGrid.g0[idx]=grid.g0[idx]- (1.0/heat_spread)*(grid.g0[idx]-weights[0]*temp);
                newGrid.g1[idx]=grid.g1[idx]- (1.0/heat_spread)*(grid.g1[idx]-weights[1]*temp);
                newGrid.g2[idx]=grid.g2[idx]- (1.0/heat_spread)*(grid.g2[idx]-weights[2]*temp);
                newGrid.g3[idx]=grid.g3[idx]- (1.0/heat_spread)*(grid.g3[idx]-weights[3]*temp);
                newGrid.g4[idx]=grid.g4[idx]- (1.0/heat_spread)*(grid.g4[idx]-weights[4]*temp);
                newGrid.g5[idx]=grid.g5[idx]- (1.0/heat_spread)*(grid.g5[idx]-weights[5]*temp);
                newGrid.g6[idx]=grid.g6[idx]- (1.0/heat_spread)*(grid.g6[idx]-weights[6]*temp);
                newGrid.g7[idx]=grid.g7[idx]- (1.0/heat_spread)*(grid.g7[idx]-weights[7]*temp);
                newGrid.g8[idx]=grid.g8[idx]- (1.0/heat_spread)*(grid.g8[idx]-weights[8]*temp);
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

            // g0: Rest, stays at the same cell
            int n0 = getIndex(x + cx[0], y + cy[0]);
            gridNew.g0[n0] = gridOld.g0[currentIndex];

            // g1: Right
            int n1 = getIndex(x + cx[1], y + cy[1]);
            gridNew.g1[n1] = gridOld.g1[currentIndex];

            // g2: Up
            int n2 = getIndex(x + cx[2], y + cy[2]);
            gridNew.g2[n2] = gridOld.g2[currentIndex];

            // g3: Left
            int n3 = getIndex(x + cx[3], y + cy[3]);
            gridNew.g3[n3] = gridOld.g3[currentIndex];

            // g4: Down
            int n4 = getIndex(x + cx[4], y + cy[4]);
            gridNew.g4[n4] = gridOld.g4[currentIndex];

            // g5: Up-Right
            int n5 = getIndex(x + cx[5], y + cy[5]);
            gridNew.g5[n5] = gridOld.g5[currentIndex];

            // g6: Up-Left
            int n6 = getIndex(x + cx[6], y + cy[6]);
            gridNew.g6[n6] = gridOld.g6[currentIndex];

            // g7: Down-Left
            int n7 = getIndex(x + cx[7], y + cy[7]);
            gridNew.g7[n7] = gridOld.g7[currentIndex];

            // g8: Down-Right
            int n8 = getIndex(x + cx[8], y + cy[8]);
            gridNew.g8[n8] = gridOld.g8[currentIndex];
        }
    }
}