#pragma once
#include <sched.h>

#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "../EventLoop.h"
#include "../Loggers/Logger.h"
#include "../helpers/queue.h"
#include "Thread.h"

#define _load_relaxed(ptr)                        __atomic_load_n(ptr, __ATOMIC_RELAXED)
#define _load_acquire(ptr)                        __atomic_load_n(ptr, __ATOMIC_ACQUIRE)
#define _store_relaxed(ptr, val)                  __atomic_store_n(ptr, val, __ATOMIC_RELAXED)
#define _store_release(ptr, val)                  __atomic_store_n(ptr, val, __ATOMIC_RELEASE)
#define _fetch_add_release(ptr, val)              __atomic_fetch_add(ptr, val, __ATOMIC_RELEASE)
#define _CAS_weak_relaxed(ptr, expected, desired) __atomic_compare_exchange_n(ptr, expected, desired, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED)

typedef std::function<void()> Task;

class ThreadPool
{
   public:
    ThreadPool(uint64_t maxQueueSize_, const std::string& threadName_, int threadNum_);
    ~ThreadPool();

    void setThreadNum(int threadNum_) { threadNum = threadNum_; }

    void start();
    void stop();
    void submit(Task task_);

    void worker_loop();

   private:
    bool running;
    uint64_t queueSize;
    mpmc_bounded_Queue<Task> taskQueue;
    std::string threadName;
    int threadNum;
    std::vector<std::unique_ptr<Thread>> threads;
};