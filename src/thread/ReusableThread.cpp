#include "ReusableThread.h"
#include <iostream>
#include <memory>

// Main Writer: Berke
// Reviewer: 
// Contributers:
//
/** Gets the next task in the queue to be executed. Sleeps the thread if none are currently available.
It is meant to only be called from the ReusableThread, calling from main thread may lead to a deadlock. */
Task TaskQueue::getNextTask() {
    std::unique_lock<std::mutex> lock(this->taskMutex);

    // Sleep the thread until either we decide to stop or there is a task available.
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
//
/** Submits a task to be executed at a later point by the ReusableThread. Can be called from any thread. */
void TaskQueue::submitTask(Task task) {
    this->taskMutex.lock();

    this->queue.push(task);
    this->condition.notify_one(); // Wake up a thread if one is sleeping. We have one thread right now so it doesn't matter whether we use notify_one or notify_all.

    this->taskMutex.unlock();
}

// Main Writer: Berke
// Reviewer: 
// Contributers:
/**
 * Signals the TaskQueue to stop accepting new tasks. If the thread is waiting for a task, it will be woken up and returned a nullptr.
 * Called when the thread is about to termiated. Can be called from any thread.
 */
void TaskQueue::stop() {
    this->stopping = true;
    this->condition.notify_all();
}

// Main Writer: Berke
// Reviewer: 
// Contributers:
/**
 * Terminate the thread. It will signal the thread that it needs to stop, and when it is done executing any tasks (if any were ongoing), the ReusableThread will be joined with the calling thread.
 * It can be called from any thread.
 */
void ReusableThread::terminate() {
    if (this->terminateNext == true) return;

    this->terminateNext = true;
    this->taskQueue.stop();

    this->thread.join();
}

// Main Writer: Berke
// Reviewer: 
// Contributers:
ReusableThread::ReusableThread(std::unique_ptr<SimulationState> initialState): buffers(std::move(initialState)) {
    this->thread = std::thread(&ReusableThread::threadMain, this); // Launch the thread.
}

// Main Writer: Berke
// Reviewer: 
// Contributers:
/**
 * Submit a task to be executed at a later point. Can be called from any thread.
 */
void ReusableThread::submitTask(Task task) {
    this->taskQueue.submitTask(task);
}

// Main Writer: Berke
// Reviewer: 
// Contributers:
/**
 * Gets the current SimulationStatePointer. This is the preferred way of getting the state of the simulation. Can be called from any thread.
 */
const SimulationState* ReusableThread::getState() {
    std::lock_guard<std::mutex> lock(this->stateMutex);

    if (this->bufferSwapNeeded) {
        this->bufferSwapNeeded = false;
        return this->buffers.swapPrevious();
    }
    else {
        return this->buffers.getPrevious();
    }

}

// Main Writer: Berke
// Reviewer: 
// Contributers:
/**
 * This is the entry point for the compute thread.
 * It consists of a loop that continually checks if a task is available, and if so executes it and modifies the current state according to the result.
 */
void ReusableThread::threadMain() {
    beginning:

    Task task = this->taskQueue.getNextTask();

    if (terminateNext) {
        return;
    }

    const SimulationState* previousState = this->buffers.getPrevious();
    std::__1::unique_ptr<SimulationState>& nextState = this->buffers.getNext();
    task(*previousState, *nextState);

    stateMutex.lock();
    this->buffers.swapNext();
    this->bufferSwapNeeded = true;
    stateMutex.unlock();

    goto beginning; // I prefer this to having the entire method indented in a while true loop.
}

// Main Writer: Kristian
// Reviewer: Berke
// Contributers:
ReusableThread::~ReusableThread() {
    this->terminate();
}