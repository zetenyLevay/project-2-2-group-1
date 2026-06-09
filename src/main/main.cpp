#include "main.h"
#include "ui.h"
#include "../data/local/LocalEngine.h"
#include "../data/cavity/LidDrivenCavity.h"
#include "../data/BatchRunner.h"
//#include <bits/stdc++.h>
#include <iostream>
#include <vector>
#include <iomanip>
#include <memory>
#include <string>

// Main Writer: Gecenio
// Reviewer: 
// Contributers: Kristian
int main(int argc, char* argv[]) {
    if (argc >= 2 && std::string(argv[1]) == "--batch") {
        // runSimulations(int width, int height, int temperature, bool constantHeatSource, int NumberOfSims, const std::string& filename)
        // Command should be: .\project_2_2_group_1.exe --batch <width> <height> <temperature> <constantHeat> <NumberOfSims> <filename>
        // Default command: .\project_2_2_group_1.exe --batch <width> <height> <NumberOfSims> <filename>

        if (argc == 6) {
            int width = atoi(argv[2]);
            int height = atoi(argv[3]);
            int NumberOfSims = atoi(argv[4]);
            std::string filename = argv[5];

            std::thread batchThread = runSimulations(width, height, 55.0, true, NumberOfSims, filename);
            batchThread.join(); // Keeps thread alive until it is finished
        }
        else if (argc == 8) {
            int width = atoi(argv[2]);
            int height = atoi(argv[3]);
            int temperature = atoi(argv[4]);
            bool constantHeat = true;
            if (std::string(argv[5]) == "false") { constantHeat = false; }
            int NumberOfSims = atoi(argv[6]);
            std::string filename = argv[7];

            std::thread batchThread = runSimulations(width, height, temperature, constantHeat, NumberOfSims, filename);
            batchThread.join(); // Keeps thread alive until it is finished
        }
        else {
            std::cerr << "Incorrect number of arguements for batch simulation" << std::endl;
            std::cerr << "Default: "<< argv[0] << " --batch <width> <height> <NumberOfSims> <filename>" << std::endl;
            std::cerr << "Specific: "<< argv[0] << " --batch <width> <height> <temperature> <constantHeat> <NumberOfSims> <filename>" << std::endl;
            return 1;
        }
        if (argc < 8) {
            
        }
    }
    else if (argc >= 2 && std::string(argv[1]) == "--cavity") {
        // ./project_2_2_group_1 --cavity [N] [Re] [U_lid]
        int N = (argc >= 3) ? std::atoi(argv[2]) : 64;
        double Re = (argc >= 4) ? std::atof(argv[3]) : 100.0;
        double U_lid = (argc >= 5) ? std::atof(argv[4]) : 0.1;

        std::cout << "=== Lid-Driven Cavity Benchmark ===\n"
                  << "N: " << N << "\n"
                  << "Re: " << Re << "\n"
                  << "U_lid: " << U_lid << "\n"
                  << "===================================\n";

        runCavityBenchmark(N, Re, U_lid);
    }
    else {
        // Making this default but later it should be opened with .\project_2_2_group_1.exe --ui
        std::cout << "Booting Desktop UI..." << std::endl;
        #if CUDA_AVAILABLE == 1
        startGui(DataSource::LOCAL_CUDA);
        #else
        startGui(DataSource::LOCAL);
        #endif
    }
    return 0;
}