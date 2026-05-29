#pragma once
#include <pthread.h>
#include <sched.h>

#include <cstdint>
#include <functional>
#include <string>

#include "CountDownLatch.h"

typedef std::function<void()> ThreadFunc;

class Thread
{
   public:
    explicit Thread(ThreadFunc func, const std::string& name);

    bool started_() { return started; }
    void start();
    int join();

   private:
    void setDefaultName();
    bool started;
    bool joined_;
    pthread_t pthread_id;
    pid_t pid;
    std::string name;
    CountDownLatch latch;
    ThreadFunc threadFunc;
    uint32_t ThreadNum;
};