#include "EventloopThreadPool.h"

#include <cassert>
#include <cstdio>
#include <memory>

#include "EventLoop.h"
#include "EventloopThread.h"
#include "Logger.h"

EventloopThreadPool::EventloopThreadPool(EventLoop* baseLoop_, const std::string& threadNume_) : baseLoop(baseLoop_), started(false), threadName(threadNume_), threadNum(-1), next(0) {}

EventloopThreadPool::~EventloopThreadPool() {}

void EventloopThreadPool::start(const ThreadInitCallback& cb_)
{
    assert(!started);
    baseLoop->assertInLoopThread();

    char buf[16];
    for (int i = 0; i < threadNum; i++)
    {
        snprintf(buf + threadName.size(), sizeof(buf) - threadName.size(), "%d", i);
        threads.push_back(std::make_unique<EventloopThread>(threadName + buf, cb_));
        loops.push_back(threads[i]->startLoop());
    }
    if (threadNum == 0 && cb_)
    {
        cb_(baseLoop);
    }
    started = true;
}

std::vector<EventLoop*>& EventloopThreadPool::get_loops() { return loops; }

EventLoop* EventloopThreadPool::getNextLoop()
{
    LOG_TRACE << "EventloopThreadPool::getNextLoop";
    assert(started);
    baseLoop->assertInLoopThread();
    if (loops.empty()) return baseLoop;

    next++;
    next %= loops.size();
    return loops[next];
}