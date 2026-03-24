#include "main.h"

int getIndex(int x, int y) {
    int wrappedX = (x + WIDTH) % WIDTH;
    int wrappedY = (y + HEIGHT) % HEIGHT;
    return wrappedY * WIDTH + wrappedX;
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