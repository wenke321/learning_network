
#include "EventLoop.h"

#include <pthread.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>

#include "Channel.h"
#include "Logger.h"
#include "Poller.h"
#include "TimerQueue.h"

const int PollTimeout = 10000;

__thread EventLoop* loopInThisThread = 0;

EventLoop* loopInThisThread_() { return loopInThisThread; }

int createEventfd()
{
    int evfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evfd < 0)
    {
        LOG_ERROR << " wakeup fd create failed !";
        abort();
    }
    LOG_TRACE << " evfd=" << evfd;
    return evfd;
}

EventLoop::EventLoop() : quit(false), looping(false), eventHandling(false), callingPendingFunctors(false), wakeup_fd(createEventfd()), tid_(CurrentThread::tid()), epoller(new Epoller(this)), wakeupChannel(add_channel(wakeup_fd)), cur_activeCh(NULL), timerQueue(std::make_unique<TimerQueue>(this))
{
    LOG_DEBUG << "EventLoop created " << this << " in thread " << tid_;
    if (loopInThisThread)
    {
        LOG_FATAL << "Another EventLoop " << loopInThisThread << " exists in this thread " << tid_;
    }
    else
    {
        loopInThisThread = this;
    }
    wakeupChannel->set_read_callback([&] { handleRead(); });
    wakeupChannel->EnableRead();
}

EventLoop::~EventLoop()
{
    for (auto it : activeChannels)
    {
    }
}

bool EventLoop::isInLoopThread() { return tid_ == CurrentThread::tid(); }

void EventLoop::abortNotInLoopThread() { LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this << " was created in tid = " << tid_ << ", current thread id = " << CurrentThread::tid(); }

void EventLoop::assertInLoopThread()
{
    if (!isInLoopThread()) abortNotInLoopThread();
}

void EventLoop::Loop()
{
    assert(!looping);
    assertInLoopThread();
    {
        MutexLockGuard lock(mutex);
        if (quit == true) return;
    }

    looping = true;
    LOG_DEBUG << " start";
    while (!quit)
    {
        activeChannels.clear();
        pollReturnTime = epoller->Poll(PollTimeout, activeChannels);

        eventHandling = true;

        LOG_TRACE << "Loop handle events";
        for (Channel* active_ch : activeChannels)
        {
            cur_activeCh = active_ch;
            cur_activeCh->HandleEvent();
        }
        eventHandling          = false;
        cur_activeCh           = NULL;
        callingPendingFunctors = true;

        LOG_TRACE << " do pendingFunctors";
        for (Functor f : pendingFunctors)
        {
            f();
        }
        pendingFunctors.clear();
        callingPendingFunctors = false;
    }

    LOG_TRACE << " Loop quit";
    looping = false;
}

void EventLoop::quit_()
{
    quit = true;

    if (!isInLoopThread()) wakeup();
}

void EventLoop::runInLoop(Functor cb)
{
    LOG_DEBUG << " ";
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

void EventLoop::runAt(triggerTime_t _triggerTime, TimerCallback _cb)
{
    LOG_DEBUG << " ";
    timerQueue->addTimer(_triggerTime, _cb, 0);
}

// @_P _after:second
void EventLoop::runAfter(double _after, TimerCallback _cb)
{
    LOG_DEBUG << " ";
    timerQueue->addTimer(Timestamp::now_microsecconds() + _after * 1000000, _cb, 0);
}

// @_P _repeatCircle:second
void EventLoop::runEvery(TimerCallback _cb, double _repeatCircle)
{
    LOG_DEBUG << " ";
    timerQueue->addTimer(Timestamp::now_microsecconds(), _cb, _repeatCircle);
}

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
    LOG_DEBUG << " eventfd=" << wakeup_fd;
    uint64_t byte = 1;

    int n = sockOption::write(wakeup_fd, &byte, sizeof(byte));
    if (n != sizeof(byte))
    {
        LOG_ERROR << " EventLoop::wakeup failed!";
    }
}

void EventLoop::handleRead()
{
    LOG_DEBUG << " eventfd=" << wakeup_fd;
    uint64_t byte = 1;

    int n = sockOption::read(wakeup_fd, &byte, sizeof(byte));
    if (n != sizeof(byte))
    {
        LOG_ERROR << " EventLoop::handleRead failed";
    }
}

Channel* EventLoop::add_channel(int fd)
{
    // assertInLoopThread();
    return epoller->add_channel(fd);
}

void EventLoop::add_channel(Channel* _ch) { epoller->updateChannel(_ch); }

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