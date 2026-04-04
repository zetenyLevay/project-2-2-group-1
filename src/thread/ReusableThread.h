#pragma once

#include <thread>
#include <vector>
#include <functional>
#include <main.h>
#include <queue>
#include <memory>
#include <mutex>

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

        std::shared_ptr<SimulationState> getMutableState();

        ReusableThread(SimulationStatePointer initialState);

        void terminate();

        private:
        bool terminateNext = false;
        
        std::thread thread;
        void threadMain();
        
        SimulationStatePointer currentStatePtr;
        std::mutex stateMutex;
};