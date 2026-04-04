#include "SimulationEngine.h"

std::shared_ptr<const SimulationState> SimulationEngine::getState() {
    return thread->getState();
}