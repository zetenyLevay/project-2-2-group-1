#pragma once

#include <thread>
#include <vector>
#include <functional>
#include <main.h>
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>

class ReusableThread;

#include "../data/SimulationEngine.h"
#include "SimulationStateBuffers.h"

using Task = std::function<void(const SimulationState&, SimulationState&)>;

class TaskQueue {
    public:
        void submitTask(Task task);
        Task getNextTask();
        void stop();
    
    private:
        bool stopping = false;
        std::condition_variable condition;
        std::mutex taskMutex;
        std::queue<Task> queue;
};

class ReusableThread {
    public:
        TaskQueue taskQueue;

        void submitTask(Task task);

        const SimulationState* getState();

        ReusableThread(std::unique_ptr<SimulationState> initialState);

        ~ReusableThread();

        void terminate();

    private:
        bool terminateNext = false;
        
        std::thread thread;
        void threadMain();

        std::mutex stateMutex;

        SimulationStateBuffers buffers;
        bool bufferSwapNeeded = false;
};