#pragma once

#include "EventLoop.h"
#include "Thread.h"

typedef std::function<void(EventLoop*)> ThreadInitCallback;

class EventloopThread
{
   public:
    EventloopThread(const std::string&, ThreadInitCallback);
    ~EventloopThread();

    EventLoop* startLoop();

   private:
    EventLoop* loop;
    void threadFunc();
    bool exiting;
    Thread thread;
    MutexLock mutex;
    Condition cond;
    ThreadInitCallback cb;
};