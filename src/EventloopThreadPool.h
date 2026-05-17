#pragma once

#include <memory>
#include <vector>

#include "EventLoop.h"
#include "EventloopThread.h"
class EventloopThreadPool
{
   public:
    EventloopThreadPool(EventLoop* baseLoop_, const std::string& threadName);
    ~EventloopThreadPool();

    void start(const ThreadInitCallback& cb_);
    void stop();
    void setThreadNum(int n) { threadNum = n; }
    EventLoop* getNextLoop();
    std::vector<EventLoop*>& get_ioloops();

    bool started_() { return started; }

   private:
    EventLoop* baseLoop;
    bool started;
    std::string threadName;
    int threadNum;
    int next;
    std::vector<std::unique_ptr<EventloopThread>> threads;
    std::vector<EventLoop*> loops;
};