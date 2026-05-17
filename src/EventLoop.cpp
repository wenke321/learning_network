
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
#include "Mutex.h"
#include "Poller.h"
#include "TcpConnection.h"
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

EventLoop::EventLoop() : pendingFunctors(1024), epoller(new Epoller(this)), cur_activeCh(NULL), tid_(CurrentThread::tid()), quit(false), looping(false), eventHandling(false), callingPendingFunctors(false)
{
    LOG_INFO << "EventLoop created " << this << " in thread " << tid_;
    if (loopInThisThread)
    {
        LOG_FATAL << "Another EventLoop " << loopInThisThread << " exists in this thread " << tid_;
    }
    else
    {
        loopInThisThread = this;
    }
    wakeup_fd     = createEventfd();
    wakeupChannel = add_channel(wakeup_fd);
    wakeupChannel->set_in_callback([=] { handle_wakeup(); });
    wakeupChannel->EnableRead();

    timerQueue = std::make_unique<TimerQueue>(this);
}

EventLoop::~EventLoop()
{
    {
        LOG_DEBUG << " quit=" << quit << ",looping=" << looping;
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
    looping = true;
    LOG_INFO << "start";
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
        Functor task;
        while (!pendingFunctors.empty())
        {
            if (pendingFunctors.try_pop(task))
                task();
            else
            {
                LOG_DEBUG << " pendingFunctors pop failed";
            }
        }
        callingPendingFunctors = false;
    }

    {
        LOG_DEBUG << " Loop quit";
    }
    looping = false;
}

void EventLoop::quit_()
{
    quit = true;

    if (!isInLoopThread()) wakeup();
}

void EventLoop::runInLoop(Functor _cb)
{
    if (isInLoopThread())
    {
        _cb();
    }
    else
        queueInLoop(_cb);
}

void EventLoop::queueInLoop(Functor _cb)
{
    pendingFunctors.push(_cb);

    if (!isInLoopThread()) wakeup();
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
    LOG_TRACE << " eventfd=" << wakeup_fd;
    uint64_t byte = 1;

    int n = sockOption::write(wakeup_fd, &byte, sizeof(byte));
    if (n != sizeof(byte))
    {
        LOG_ERROR << " EventLoop::wakeup failed!";
    }
}

void EventLoop::handle_wakeup()
{
    LOG_TRACE << " eventfd=" << wakeup_fd;
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