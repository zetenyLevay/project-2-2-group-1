#pragma once

#include <thread>
#include <vector>
#include <functional>
#include <main.h>
#include <queue>

class ReusableThread;

#include "../data/SimulationEngine.h"

using SimulationStatePointer = std::shared_ptr<const SimulationState>;
using Task = std::function<void(SimulationState &)>;

class TaskQueue {
    public:
        void submitTask(Task task);
        Task getNextTask();
    
    private:
        std::mutex taskMutex;
        std::queue<Task> queue;
};

class ReusableThread {
    public:
        TaskQueue taskQueue;

        void submitTask(Task task);

        SimulationStatePointer getState();

        ReusableThread(SimulationStatePointer initialState);

        private:
        std::thread thread;
        void threadMain();
        
        SimulationStatePointer currentStatePtr;
        std::mutex stateMutex;
};