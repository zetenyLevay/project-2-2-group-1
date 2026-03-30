//
// Created by levay on 3/23/2026.
//

#include "main.h"
#include "ui.h"
#include <iostream>
#include <vector>
#include <iomanip>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;

void runWebSocketServer(Grid& grid, Grid& gridTemp, std::vector<double>& temperatures) {
    server ws_server;
    ws_server.clear_access_channels(websocketpp::log::alevel::all);
    ws_server.init_asio();

    ws_server.set_message_handler([&](websocketpp::connection_hdl hdl, server::message_ptr msg) {
        if (msg->get_payload() == "NEXT_FRAME") {
            Collision(grid, gridTemp, 1.0); // heat_spread = 1.0
            stream(gridTemp, grid); 
            
            for (int i = 0; i < CELLS; i++) {
                temperatures[i] = grid.g0[i] + grid.g1[i] + grid.g2[i] + 
                                  grid.g3[i] + grid.g4[i] + grid.g5[i] + 
                                  grid.g6[i] + grid.g7[i] + grid.g8[i];
            }
            ws_server.send(hdl, temperatures.data(), temperatures.size() * sizeof(double), websocketpp::frame::opcode::binary);
        }
    });

    std::cout << "Starting LBM Physics Server on ws://localhost:8080..." << std::endl;
    ws_server.listen(8080);
    ws_server.start_accept();
    ws_server.run(); 
}

// Physics constants
const double MAX_TEMP = 100.0; // temp of top left cell
const double ROOM_TEMP = 20.0;
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
    Grid grid;
    Grid gridTemp;


    std::vector<double> temperatures(CELLS, ROOM_TEMP);

    temperatures[getIndex(0,0)] = MAX_TEMP;

    for (int i = 0; i < CELLS; i++) {
        grid.g0[i] = weights[0] * temperatures[i];
        grid.g1[i] = weights[1] * temperatures[i];
        grid.g2[i] = weights[2] * temperatures[i];
        grid.g3[i] = weights[3] * temperatures[i];
        grid.g4[i] = weights[4] * temperatures[i];
        grid.g5[i] = weights[5] * temperatures[i];
        grid.g6[i] = weights[6] * temperatures[i];
        grid.g7[i] = weights[7] * temperatures[i];
        grid.g8[i] = weights[8] * temperatures[i];
    }
    
    // printTemperatures(temperatures, 0);

    // Mode Selector
    bool run_gui_mode = true; // Set to false to run the WebSocket server!

    if (run_gui_mode) {
        std::cout << "Booting Desktop UI..." << std::endl;
        startGui(grid, gridTemp, temperatures);
    } else {
        runWebSocketServer(grid, gridTemp, temperatures);
    }

    return 0;
}