#include "main.h"
#include <iostream>
#include <cassert>

int main() {
    /*
    std::cout << "Starting Streaming Tests..." << std::endl;

    Grid gridOld;
    Grid gridNew;

    // Mock data
    // Inject a packet at (0,0) moving RIGHT (direction 1). 
    // Remember cx[1] = 1, cy[1] = 0
    gridOld.g[1][getIndex(0, 0)] = 99.0;

    // Inject a packet at (0,0) moving LEFT (direction 3).
    // Remember cx[3] = -1. Because of getIndex wrapping, x=-1 becomes x=2.
    gridOld.g[3][getIndex(0, 0)] = 55.0;

    // Run stream
    stream(gridOld, gridNew);

    // Actual testing
    // The packet at (0,0) moving right should now be at (1,0) in g1.
    assert(gridNew.g[1][getIndex(1, 0)] == 99.0);
    
    // The packet at (0,0) moving left should have wrapped around to (2,0) in g3.
    assert(gridNew.g[3][getIndex(2, 0)] == 55.0);

    // Ensure it left the original spot
    assert(gridNew.g[1][getIndex(0, 0)] == 0.0);

    std::cout << "SUCCESS: All streaming tests passed!" << std::endl;
    return 0;
    */
}