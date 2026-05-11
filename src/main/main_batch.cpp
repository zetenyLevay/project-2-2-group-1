// Batch-only entry point — no GUI, no WebSocket dependencies.
#include "main.h"
#include "../data/BatchRunner.h"
#include "../data/cavity/LidDrivenCavity.h"
#include <iostream>
#include <thread>
#include <string>
#include <cstdlib>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " --batch <width> <height> <NumberOfSims> <filename> <saveType>\n"
                  << "  " << argv[0] << " --cavity [N] [Re] [U_lid]\n";
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "--batch") {
        if (argc < 7) {
            std::cerr << "Too few arguments for batch simulation:\n"
                      << argv[0] << " --batch <width> <height> <NumberOfSims> <filename> <saveType>\n"
                      << "  saveType: 0 = Necessary, 1 = Complete" << std::endl;
            return 1;
        }

        int width = std::atoi(argv[2]);
        int height = std::atoi(argv[3]);
        int NumberOfSims = std::atoi(argv[4]);
        std::string filename = argv[5];
        int selectedSave = std::atoi(argv[6]);

        SaveType saveType = selectedSave == 0 ? SaveType::NECESSARY : SaveType::COMPLETE;

        std::thread batchThread = runSimulations(width, height, NumberOfSims, filename, saveType);
        batchThread.join();
        return 0;
    }
    else if (mode == "--cavity") {
        int N = (argc >= 3) ? std::atoi(argv[2]) : 64;
        double Re = (argc >= 4) ? std::atof(argv[3]) : 100.0;
        double U_lid = (argc >= 5) ? std::atof(argv[4]) : 0.1;

        runCavityBenchmark(N, Re, U_lid);
        return 0;
    }
    else {
        std::cerr << "Unknown mode: " << mode << "\n"
                  << "Usage:\n"
                  << "  " << argv[0] << " --batch <width> <height> <NumberOfSims> <filename> <saveType>\n"
                  << "  " << argv[0] << " --cavity [N] [Re] [U_lid]\n";
        return 1;
    }
}
