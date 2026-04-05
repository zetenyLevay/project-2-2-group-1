#include "ReusableThread.h"
#include <iostream>
#include <memory>

Task TaskQueue::getNextTask() {
    std::unique_lock<std::mutex> lock(this->taskMutex);

    this->condition.wait(lock, [this] { 
        return (this->queue.size() != 0) || (this->stopping); 
    });

    if (this->stopping) {
        return nullptr;
    }

    Task task = this->queue.front();
    this->queue.pop();

    return task;
}

void TaskQueue::submitTask(Task task) {
    this->taskMutex.lock();

    this->queue.push(task);
    this->condition.notify_one();

    this->taskMutex.unlock();
}

void TaskQueue::stop() {
    this->stopping = true;
    this->condition.notify_one();
}

void ReusableThread::terminate() {
    if (this->terminateNext == true) return;

    this->terminateNext = true;
    this->taskQueue.stop();

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
    std::shared_ptr<SimulationState> statePtr = std::const_pointer_cast<SimulationState>(this->currentStatePtr);

    return statePtr;
}

void ReusableThread::threadMain() {
    beginning:

    Task task = this->taskQueue.getNextTask();

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

ReusableThread::~ReusableThread() {
    this->terminate();
}