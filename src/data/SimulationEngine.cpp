#include "SimulationEngine.h"

SimulationEngine::SimulationEngine(int w, int h) : width(w), height(h), cells(w * h) {};

std::shared_ptr<const SimulationState> SimulationEngine::getState() {
    return this->thread->getState();
}

std::shared_ptr<SimulationState> SimulationEngine::getMutableState() {
    return this->thread->getMutableState();
}

// Main Writer: Gecenio
// Reviewer: 
// Contributers: Berke 
const int SimulationEngine::getIndex(int x, int y) {
    int wrappedX = (x + this->width) % this->width;
    int wrappedY = (y + this->height) % this->height;
    return wrappedY * this->width + wrappedX;
}