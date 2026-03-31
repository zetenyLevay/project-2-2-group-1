//
// Created by levay on 3/23/2026.
//

#include "main.h"
#include "ui.h"
#include "SimulationEngine.h"
#include <iostream>
#include <vector>
#include <iomanip>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;

void runWebSocketServer(SimulationEngine& engine) {
    server ws_server;
    ws_server.clear_access_channels(websocketpp::log::alevel::all);
    ws_server.init_asio();

    ws_server.set_message_handler([&](websocketpp::connection_hdl hdl, server::message_ptr msg) {
        if (msg->get_payload() == "NEXT_FRAME") {
            // Advance the physics by one frame
            engine.stepFoward();
            
            // Send updated temperatures
            ws_server.send(hdl, engine.temperatures.data(), engine.temperatures.size() * sizeof(double), websocketpp::frame::opcode::binary);
        }
    });

    std::cout << "Starting LBM Physics Server on ws://localhost:8080..." << std::endl;
    ws_server.listen(8080);
    ws_server.start_accept();
    ws_server.run(); 
}


// relaxation time for temperature spread
const double heat_spread = 1.0;


// Helper to print the grid cleanly (ai)
void printTemperatures(const std::vector<double>& temps, int step) {
    std::cout << "--- Time Step " << step << " ---" << std::endl;
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            std::cout << std::fixed << std::setprecision(2) << std::setw(8) << temps[getIndex(x, y)];
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

int main() {
    SimulationEngine engine;

    engine.temperatures[getIndex(0,0)] = MAX_TEMP;

    for (int i = 0; i < CELLS; i++) {
        for (int d = 0; d < 9; ++d) {
            engine.grid.g[d][i] = weights[d] * engine.temperatures[i];
        }
    }
    
    // printTemperatures(temperatures, 0);

    // Mode Selector
    bool run_gui_mode = true; // Set to false to run the WebSocket server!

    if (run_gui_mode) {
        std::cout << "Booting Desktop UI..." << std::endl;
        startGui(engine);
    } else {
        runWebSocketServer(engine);
    }

    return 0;
}