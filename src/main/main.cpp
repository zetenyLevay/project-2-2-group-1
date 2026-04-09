#include "main.h"
#include "ui.h"
#include "../data/local/LocalEngine.h"
#include "../data/BatchRunner.h"
//#include <bits/stdc++.h>
#include <iostream>
#include <vector>
#include <iomanip>
#include <memory>
#include <string>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;

// Main Writer: Gecenio
// Reviewer: 
// Contributers: 
void runWebSocketServer(LocalEngine& engine) {
    server ws_server;
    ws_server.clear_access_channels(websocketpp::log::alevel::all);
    ws_server.init_asio();

    ws_server.set_message_handler([&](websocketpp::connection_hdl hdl, server::message_ptr msg) {
        if (msg->get_payload() == "NEXT_FRAME") {
            // Advance the physics by one frame
            engine.stepFoward();

            auto state = engine.getState();
            
            // Send updated temperatures
            ws_server.send(hdl, state->temperatures.data(), state->temperatures.size() * sizeof(double), websocketpp::frame::opcode::binary);
        }
    });

    std::cout << "Starting LBM Physics Server on ws://localhost:8080..." << std::endl;
    ws_server.listen(8080);
    ws_server.start_accept();
    ws_server.run(); 
}

// relaxation time for temperature spread
const double heat_spread = 1.0;

// Main Writer: Gecenio
// Reviewer: 
// Contributers: Kristian
int main(int argc, char* argv[]) {
    if (argc >= 2 && std::string(argv[1]) == "--batch") {
        // runSimulations(int width, int height, int NumberOfSims, const std::string& filename, SaveType saveType)
        // Command should be: .\project_2_2_group_1.exe --batch <width> <height> <NumberOfSims> <filename> <saveType>

        if (argc < 7) {
            std::cerr << "Too few arguements for batch simulation: \n" << argv[0] << " --batch <width> <height> <NumberOfSims> <filename> <saveType>" << std::endl;
            return 1;
        }

        int width = atoi(argv[2]);
        int height = atoi(argv[3]);
        int NumberOfSims = atoi(argv[4]);
        std::string filename = argv[5];
        int selectedSave = atoi(argv[6]);
        
        SaveType saveType = selectedSave == 0 ? SaveType::NECESSARY : SaveType::COMPLETE;

        std::thread batchThread = runSimulations(width, height, NumberOfSims, filename, saveType);
        batchThread.join(); // Keeps thread alive until it is finished
    }
    else {
        // Making this default but later it should be opened with .\project_2_2_group_1.exe --ui
        std::cout << "Booting Desktop UI..." << std::endl;
        startGui(DataSource::LOCAL);
    }
    return 0;
}