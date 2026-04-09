#include "ReusableThread.h"
#include <iostream>
#include <memory>

// Main Writer: Berke
// Reviewer: 
// Contributers:
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

// Main Writer: Berke
// Reviewer: 
// Contributers:
void TaskQueue::submitTask(Task task) {
    this->taskMutex.lock();

    this->queue.push(task);
    this->condition.notify_one();

    this->taskMutex.unlock();
}

// Main Writer: Berke
// Reviewer: 
// Contributers:
void TaskQueue::stop() {
    this->stopping = true;
    this->condition.notify_one();
}

// Main Writer: Berke
// Reviewer: 
// Contributers:
void ReusableThread::terminate() {
    if (this->terminateNext == true) return;

    this->terminateNext = true;
    this->taskQueue.stop();

    if (this->thread.get_id() == std::this_thread::get_id()) {
        std::cerr << "uh oh, tried to terminate own thread, this is a deadlock!!!\n";
    }

    this->thread.join();
}

// Main Writer: Berke
// Reviewer: 
// Contributers:
ReusableThread::ReusableThread(SimulationStatePointer initialState) {
    this->currentStatePtr = initialState;
    thread = std::thread(&ReusableThread::threadMain, this);
}

// Main Writer: Berke
// Reviewer: 
// Contributers:
void ReusableThread::submitTask(Task task) {
    this->taskQueue.submitTask(task);
}

// Main Writer: Berke
// Reviewer: 
// Contributers:
SimulationStatePointer ReusableThread::getState() {
    SimulationStatePointer statePtr;
    this->stateMutex.lock();
    statePtr = this->currentStatePtr;
    this->stateMutex.unlock();
    return statePtr;
}

// Main Writer: Berke
// Reviewer: 
// Contributers:
std::shared_ptr<SimulationState> ReusableThread::getMutableState() {
    std::shared_ptr<SimulationState> statePtr = std::const_pointer_cast<SimulationState>(this->currentStatePtr);

    return statePtr;
}

// Main Writer: Berke
// Reviewer: 
// Contributers:
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

// Main Writer: Berke
// Reviewer: 
// Contributers:
ReusableThread::~ReusableThread() {
    this->terminate();
}