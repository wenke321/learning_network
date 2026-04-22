#include "ThreadPool.h"

#include <cassert>
#include <cstdio>
#include <memory>

#include "Thread.h"

ThreadPool::ThreadPool(uint64_t maxQueueSize_, const std::string& threadName_, int threadNum_) : running(false), queueSize(maxQueueSize_), taskQueue(queueSize), threadName(threadName_), threadNum(threadNum_) {}

ThreadPool::~ThreadPool()
{
    if (running) stop();
}

void ThreadPool::start()
{
    assert(threads.empty());
    running = true;

    for (int i = 0; i < threadNum; i++)
    {
        char id[32];
        snprintf(id, sizeof(id), "%d", i);
        threads.push_back(std::make_unique<Thread>([this] { worker_loop(); }, threadName + id));
        threads[i]->start();
    }
}

void ThreadPool::stop()
{
    running = false;
    for (int i = 0; i < threadNum; i++) threads[i]->join();
}

void ThreadPool::submit(Task task_) { taskQueue.push(task_); }

void ThreadPool::worker_loop()
{
    while (running)
    {
        Task task;
        if (taskQueue.pop(task))
            task();
        else
            sched_yield();
    }
    Task task;
    while (taskQueue.pop(task)) task();
}