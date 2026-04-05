#include "main.h"
#include "ui.h"
#include "../data/local/LocalEngine.h"
#include <iostream>
#include <vector>
#include <iomanip>
#include <memory>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;

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

// I removed printTemperatures() because I didn't want to put in the effort of porting it to use SimulationState

int main() {
    // Mode Selector
    bool run_gui_mode = true; // Set to false to run the WebSocket server!
    if (run_gui_mode) {
        std::cout << "Booting Desktop UI..." << std::endl;
        startGui(DataSource::LOCAL);
    } else {
        // TODO: Fix web socket call
        //runWebSocketServer(engine);
    }

    return 0;
}