
#include "EventLoop.h"

#include <pthread.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "Channel.h"
#include "Epoller.h"
#include "Logger.h"
#include "TimerQueue.h"

const int PollTimeout = 10000;

int createEventfd()
{
    int evfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evfd < 0)
    {
        // log
        abort();
    }
    return evfd;
}

EventLoop::EventLoop() : quit(false), looping(false), eventHandling(false), callingPendingFunctors(false), wakeup_fd(createEventfd()), thread_id(pthread_self()), epoller(new Epoller(this)), wakeupChannel(new Channel(wakeup_fd, this)), cur_activeCh(NULL) {}

EventLoop::~EventLoop() {}

void EventLoop::Loop()
{
    assert(!looping);
    assertInLoopThread();
    {
        MutexLockGuard lock(mutex);
        if (quit == true) return;
    }
    looping = true;
    while (!quit)
    {
        activeChannels.clear();
        pollReturnTime = epoller->Poll(PollTimeout, activeChannels);

        eventHandling = true;

        LOG_DEBUG << "Loop handle events";
        for (Channel* active_ch : activeChannels)
        {
            cur_activeCh = active_ch;
            cur_activeCh->HandleEvent();
        }
        eventHandling          = false;
        cur_activeCh           = NULL;
        callingPendingFunctors = true;

        LOG_DEBUG << " do pendingFunctors";
        for (Functor f : pendingFunctors)
        {
            f();
            pendingFunctors.clear();
        }
        callingPendingFunctors = false;
    }

    // log
    looping = false;
}

void EventLoop::quit_()
{
    quit = true;

    if (!isInLoopThread()) wakeup();
}

void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread())
        cb();
    else
        queueInLoop(cb);
}

void EventLoop::queueInLoop(Functor cb)
{
    {
        MutexLockGuard lock(mutex);
        pendingFunctors.push_back(cb);
    }

    if (!isInLoopThread() || callingPendingFunctors) wakeup();
}

void EventLoop::runAt(triggerTime_t triggerTime, TimerCallback cb) { timerQueue->addTimer(triggerTime, cb, 0); }
void EventLoop::runAfter(triggerTime_t after, TimerCallback cb) { timerQueue->addTimer(Timestamp::now_microsecconds() + after, cb, 0); }
void EventLoop::runEvery(triggerTime_t triggerTime_, TimerCallback cb, double repeatCircle_) { timerQueue->addTimer(triggerTime_, cb, repeatCircle_); }

void EventLoop::addTimer(Timer* timer_)
{
    assertInLoopThread();
    timerQueue->addTimer(timer_);
}

void EventLoop::cancelTimer(Timer* timer_)
{
    assertInLoopThread();
    timerQueue->cancelTimer(timer_, 0);
}

void EventLoop::wakeup()
{
    uint64_t byte = 1;

    int n = write(wakeup_fd, &byte, sizeof(byte));
    if (n != sizeof(byte))
    {
        // log
    }
}

void EventLoop::handleRead()
{
    uint64_t byte = 1;

    int n = read(wakeup_fd, &byte, sizeof(byte));
    if (n != sizeof(byte))
    {
        // log
    }
}

void EventLoop::updateChannel(Channel* ch)
{
    assert(ch->owner_loop() == this);
    assertInLoopThread();
    epoller->updateChannel(ch);
}

void EventLoop::removeChannel(Channel* ch)
{
    assert(ch->owner_loop() == this);
    assertInLoopThread();
    epoller->removeChannel(ch);
}

bool EventLoop::hasChannal(Channel* ch)
{
    assert(ch->owner_loop() == this);
    assertInLoopThread();
    return epoller->hasChannal(ch);
}