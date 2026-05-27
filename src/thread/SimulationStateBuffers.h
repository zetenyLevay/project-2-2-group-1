#pragma once
#include "../data/SimulationEngine.h"

class SimulationStateBuffers {
    public:
        const SimulationState* getPrevious();
        std::unique_ptr<SimulationState>& getCurrent();
        std::unique_ptr<SimulationState>& getNext();

        // Swap the previous and current buffers, returns (previously) current buffer.
        // To be used when returning data to the main thread.
        const SimulationState* swapPrevious();

        // Swap the current and next buffers, returns (previously) current buffer.
        // To be used when data thread completes a simulation step.
        std::unique_ptr<SimulationState>& swapNext();

        const SimulationState* getMostRecent();

        SimulationStateBuffers(std::unique_ptr<SimulationState> initialState);

    private:
        std::unique_ptr<SimulationState> previous; // Current SimulationState held by the main thread.
        std::unique_ptr<SimulationState> current; // SimulationState that is ready to be passed to the main thread.
        std::unique_ptr<SimulationState> next; // SimulationState that can be safely changed by the data thread.

        bool previousIsMostRecent = true;
};