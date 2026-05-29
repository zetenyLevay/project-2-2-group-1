#include "../data/local/LocalEngine.h"
#include <iostream>
#include <cstdio>

extern void runHelloWorld();


int main() {
	
	int width = 4;
	int height = 4;
	int cells = width * height;

	// Create Engine
	LocalEngine engine(width, height);

	// create grids
	Grid gridOld(cells);
	Grid gridNew(cells);

	//initialize gridOld with dummy values
		for (int i = 0; i < cells; i++) {
		for (int d = 0; d < 9; ++d) {
			gridOld.g[d][i] = 20.0; // constant temperature
			gridOld.f[d][i] = 1.0; // constant fluid
		}
	}

		// params
		double heat_spread = 0.8;
		double viscosity = 0.6;
		double TempAvg = 20.0;

		printf("Before Collision:\n");
		for (int i = 0; i < 9; i++)
		//print before collision
		printf("Cell 0, g[%d] = %f, f[%d] = %f\n", 0, gridOld.g[0][0], 0, gridOld.f[0][0]);

		// Run collision
		engine.Collision(heat_spread, TempAvg, viscosity, gridNew, gridOld);
		
		printf("\nAfter Collision:\n");
		// print one cell only
		for (int i = 0; i < 9; i++) {
			printf("Cell 0, g[%d] = %f, f[%d] = %f\n", i, gridNew.g[i][0], i, gridNew.f[i][0]);
		}

		runHelloWorld();


	return 0;
}