#include "EventloopThread.h"

#include <cassert>
#include <string>

#include "CountDownLatch.h"
#include "EventLoop.h"
#include "Thread.h"

EventloopThread::EventloopThread(ThreadInitCallback cb_, const std::string& name_) : loop(nullptr), exiting(false), thread([this]() { threadFunc(); }, name_), cond(mutex), cb(cb_) {}

EventloopThread::~EventloopThread()
{
    exiting = true;
    if (loop != nullptr)
    {
        loop->quit_();
        thread.join();
    }
}

EventLoop* EventloopThread::startLoop()
{
    assert(!thread.started_());
    thread.start();

    EventLoop* loop_;
    {
        MutexLockGuard lock(mutex);
        while (loop == nullptr)
        {
            cond.wait();
        }
        loop_ = loop;
    }

    return loop_;
}

void EventloopThread::threadFunc()
{
    EventLoop loop_;

    if (cb)
    {
        cb(&loop_);
    }

    {
        MutexLockGuard lock(mutex);
        loop = &loop_;
        cond.notifyAll();
    }

    loop_.Loop();

    MutexLockGuard lock(mutex);
    loop = nullptr;
}