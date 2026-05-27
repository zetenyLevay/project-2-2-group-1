#include "SimulationStateBuffers.h"

const SimulationState* SimulationStateBuffers::getPrevious() {
    return this->previous.get();
}

std::unique_ptr<SimulationState>& SimulationStateBuffers::getCurrent() {
    if (this->current == nullptr) {
        this->current = std::make_unique<SimulationState>(*(this->previous));
    }

    return this->current;
}

std::unique_ptr<SimulationState>& SimulationStateBuffers::getNext() {
    if (this->next == nullptr) {
        this->next = std::make_unique<SimulationState>(*(this->getCurrent()));
    }

    return this->next;
}

const SimulationState* SimulationStateBuffers::swapPrevious() {
    std::unique_ptr<SimulationState>& current = this->getCurrent();

    std::swap(this->previous, current);

    return this->getPrevious();
}

std::unique_ptr<SimulationState>& SimulationStateBuffers::swapNext() {
    std::unique_ptr<SimulationState>& current = this->getCurrent();
    std::unique_ptr<SimulationState>& next = this->getNext();

    std::swap(current, next);

    return next;
}

SimulationStateBuffers::SimulationStateBuffers(std::unique_ptr<SimulationState> initialState) {
    this->previous = std::move(initialState);
}