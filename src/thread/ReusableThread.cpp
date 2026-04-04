#include "ReusableThread.h"
#include <iostream>
#include <memory>

Task TaskQueue::getNextTask() {
    if (this->queue.size() == 0) return nullptr;

    this->taskMutex.lock();

    Task task = this->queue.front();
    this->queue.pop();

    this->taskMutex.unlock();

    return task;
}

void TaskQueue::submitTask(Task task) {
    this->taskMutex.lock();

    this->queue.push(task);

    this->taskMutex.unlock();
}

void ReusableThread::terminate() {
    if (this->terminateNext == true) return;

    this->terminateNext = true;

    if (this->thread.get_id() == std::this_thread::get_id()) {
        std::cerr << "uh oh, tried to terminate own thread, this is a deadlock!!!\n";
    }

    this->thread.join();
}

ReusableThread::ReusableThread(SimulationStatePointer initialState) {
    this->currentStatePtr = initialState;
    thread = std::thread(&ReusableThread::threadMain, this);
}

void ReusableThread::submitTask(Task task) {
    this->taskQueue.submitTask(task);
}

SimulationStatePointer ReusableThread::getState() {
    SimulationStatePointer statePtr;
    this->stateMutex.lock();
    statePtr = this->currentStatePtr;
    this->stateMutex.unlock();
    return statePtr;
}

std::shared_ptr<SimulationState> ReusableThread::getMutableState() {
    if (!this->terminateNext) {
        std::cerr << "Dangerous! Mutable state requested when two threads are running! This could easily cause race conditions!\n";
    }
    
    std::shared_ptr<SimulationState> statePtr = std::const_pointer_cast<SimulationState>(this->currentStatePtr);

    return statePtr;
}

void ReusableThread::threadMain() {
    std::cout << "threadMain()\n";
    beginning:

    Task task;
    while (((task = this->taskQueue.getNextTask()) == nullptr) && !this->terminateNext) {}

    if (terminateNext) {
        return;
    }

    auto previousState = this->getState();
    auto nextState = std::make_shared<SimulationState>(*previousState);
    task(*nextState);

    stateMutex.lock();
    currentStatePtr = nextState;
    stateMutex.unlock();

    goto beginning;
}