// Batch-only entry point — no GUI, no WebSocket dependencies.
#include "main.h"
#include "../data/BatchRunner.h"
#include <iostream>
#include <thread>
#include <string>
#include <cstdlib>

int main(int argc, char* argv[]) {
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
