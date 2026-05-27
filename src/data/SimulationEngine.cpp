#include "SimulationEngine.h"
#include "../thread/ReusableThread.h"
#include <stdexcept>


SimulationEngine::SimulationEngine(int w, int h) : width(w), height(h), cells(w * h) {};

bool SimulationEngine::getAutoPlayStatus() {
    return this->autoPlay;
}

void SimulationEngine::setAutoPlayStatus(bool status) {
    this->autoPlay = status;

    if (status) {
        this->stepFoward();
    }
}

/**
 * Gets the current state of the simulation.
 */
const SimulationState* SimulationEngine::getState() {
    return this->thread->getState();
}

// Main Writer: Gecenio
// Reviewer: 
// Contributers: Cosmin
const int SimulationEngine::getIndex(int x, int y) {
    if(y>=0 && y<height)
    {
        if(x>=0 && x<width)
        {
            return y * this->width + x;
        }
    }
    throw std::out_of_range("Index out of bounds");
}

const SimulationHistory* SimulationEngine::getReadOnlyHistory() {
    return this->history.get();
}

SimulationEngine::~SimulationEngine() = default;