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

// Physics constants
const double MAX_TEMP = 100.0; // temp of top left cell
const double ROOM_TEMP = 20.0;

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
        engine.grid.g0[i] = weights[0] * engine.temperatures[i];
        engine.grid.g1[i] = weights[1] * engine.temperatures[i];
        engine.grid.g2[i] = weights[2] * engine.temperatures[i];
        engine.grid.g3[i] = weights[3] * engine.temperatures[i];
        engine.grid.g4[i] = weights[4] * engine.temperatures[i];
        engine.grid.g5[i] = weights[5] * engine.temperatures[i];
        engine.grid.g6[i] = weights[6] * engine.temperatures[i];
        engine.grid.g7[i] = weights[7] * engine.temperatures[i];
        engine.grid.g8[i] = weights[8] * engine.temperatures[i];
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