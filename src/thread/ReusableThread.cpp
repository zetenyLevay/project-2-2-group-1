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

void ReusableThread::threadMain() {
    beginning:

    Task task;
    while ((task = this->taskQueue.getNextTask()) == nullptr) {}

    auto previousState = this->getState();
    auto nextState = std::make_shared<SimulationState>(*previousState);
    task(*nextState);

    stateMutex.lock();
    currentStatePtr = nextState;
    stateMutex.unlock();

    goto beginning;
}