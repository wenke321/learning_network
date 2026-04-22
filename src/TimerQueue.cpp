
#include "TimerQueue.h"

#include <sys/timerfd.h>
#include <unistd.h>

#include <cstring>
#include <memory>

#include "Timer.h"
#include "Timestamp.h"

int createTimerfd()
{
    int timer_fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd < 0)
    {
        // log err
    }
    return timer_fd;
}

void periodFromNow(struct itimerspec& newtime_, triggerTime_t& triggerTime_)
{
    triggerTime_t period = triggerTime_ - Timestamp::now_microsecconds();
    if (period < 100) period = 100;
    newtime_.it_value.tv_sec  = period / 1000000;
    newtime_.it_value.tv_nsec = period % 1000000 * 1000;
}

TimerQueue::TimerQueue(EventLoop* loop_) : own_loop(loop_), timerfd(createTimerfd()), timerChannel(timerfd, own_loop)
{
    timerChannel.set_read_callback([this] { handleRead(); });
    timerChannel.EnableRead();
}

// Timer* should be managed by who create them
TimerQueue::~TimerQueue()
{
    timerChannel.DisableAll();
    timerChannel.remove();
    ::close(timerfd);
}

void TimerQueue::addTimer(triggerTime_t triggerTime_, TimerCallback cb, double repeatCircle_)
{
    own_loop->assertInLoopThread();
    Timer* timer = new Timer(triggerTime_, cb, repeatCircle_);
    own_loop->runInLoop([this, &timer] { addTimerInLoop(timer, 0); });
}

void TimerQueue::addTimer(Timer* timer_)
{
    own_loop->assertInLoopThread();

    own_loop->runInLoop([this, &timer_] { addTimerInLoop(timer_, 0); });
}

void TimerQueue::cancelTimer(Timer* timer_, bool noOwner)
{
    own_loop->assertInLoopThread();

    own_loop->runInLoop([this, &timer_, &noOwner] { cancelTimerInLoop(timer_, noOwner); });
}

void TimerQueue::addTimerInLoop(Timer* timer_, bool noOwner)
{
    own_loop->assertInLoopThread();

    // update timerfd
    if (insert(timer_, noOwner))
    {
        //
        updateFd(timer_->TriggerTime());
    }
}

void TimerQueue::cancelTimerInLoop(Timer* timer_, bool noOwner)
{
    own_loop->assertInLoopThread();
    if (activeTimers.find(timer_->TriggerTime()) != activeTimers.end())
    {
        activeTimers[timer_->TriggerTime()].erase(timer_);
        if (noOwner) delete timer_;
    }
    else
    {
        // log illegal
    }
}

void TimerQueue::handleRead()
{
    own_loop->assertInLoopThread();

    triggeredTimers.clear();
    triggerTime_t now = Timestamp::now_microsecconds();
    for (; activeTimers.begin()->first <= now;)
    {
        for (auto it : activeTimers.begin()->second)
        {
            it.first->run();
            if (it.first->shouldRepeat())
            {
                triggeredTimers.push_back(it.first);
                continue;
            }
            if (it.second) delete it.first;
        }
        activeTimers.erase(activeTimers.begin());
    }
    reset(now);
}

bool TimerQueue::insert(Timer* timer_, bool noOwner)
{
    triggerTime_t triggerTime_ = timer_->TriggerTime();
    if (activeTimers.find(triggerTime_) != activeTimers.end())
    {
        sameTime_timers_t sameTime_timers = activeTimers[triggerTime_];
        if (sameTime_timers.find(timer_) != sameTime_timers.end())
        {
            // log illegal
        }
        else
        {
            sameTime_timers[timer_] = noOwner;
            // log success
            return activeTimers.begin()->first > triggerTime_;
        }
    }
    else
    {
        activeTimers[timer_->TriggerTime()][timer_] = noOwner;
        // log success
        return activeTimers.begin()->first >= triggerTime_;
    }
}

void TimerQueue::reset(triggerTime_t now)
{
    triggerTime_t earliest;
    for (Timer* it : triggeredTimers)
    {
        it->reset(now);
        addTimer(it);
        earliest = std::max(earliest, it->TriggerTime());
    }
    updateFd(earliest);
}

void TimerQueue::updateFd(triggerTime_t triggerTime_)
{
    struct itimerspec newtime, oldtime;
    memset(&newtime, 0, sizeof(newtime));
    memset(&oldtime, 0, sizeof(oldtime));
    periodFromNow(newtime, triggerTime_);
    if (::timerfd_settime(timerfd, 0, &newtime, &oldtime) < 0)
    {
        //
        // log timerfd_settime err
    }
}