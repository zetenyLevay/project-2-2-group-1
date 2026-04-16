#include "SimulationEngine.h"

SimulationEngine::SimulationEngine(int w, int h) : width(w), height(h), cells(w * h) {};

/**
 * Gets the current state of the simulation.
 */
SimulationStatePointer SimulationEngine::getState() {
    return this->thread->getState();
}

/**
 * Gets the current state of the simulation as a mutable pointer. This method is unsafe, use getState() whenever possible.
 */
std::shared_ptr<SimulationState> SimulationEngine::getMutableState() {
    return this->thread->getMutableState();
}

// Main Writer: Gecenio
// Reviewer: 
// Contributers: 
const int SimulationEngine::getIndex(int x, int y) {
    int wrappedX = (x + this->width) % this->width;
    int wrappedY = (y + this->height) % this->height;
    return wrappedY * this->width + wrappedX;
}