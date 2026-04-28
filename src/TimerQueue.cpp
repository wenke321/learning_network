
#include "TimerQueue.h"

#include <sys/timerfd.h>
#include <unistd.h>

#include <cstring>
#include <ctime>
#include <memory>

#include "Logger.h"
#include "Timer.h"
#include "Timestamp.h"

int createTimerfd()
{
    int timer_fd = ::timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd < 0)
    {
        LOG_ERROR << " ::timerfd_create failed !";
    }
    return timer_fd;
}

void periodFromNow(struct itimerspec& newtime_, triggerTime_t& triggerTime_)
{
    triggerTime_t t = Timestamp::now_microsecconds();
    LOG_DEBUG << " now_microsecconds=" << t;
    LOG_DEBUG << " triggerTime=" << triggerTime_;
    triggerTime_t period = 0;
    if (triggerTime_ > t) period = triggerTime_ - t;
    if (period < 100) period = 100;
    LOG_DEBUG << " period=" << period;
    newtime_.it_value.tv_sec  = period / 1000000;
    newtime_.it_value.tv_nsec = period % 1000000 * 1000;
    LOG_DEBUG << " periodFromNow=" << newtime_.it_value.tv_sec << " seconds";
}

TimerQueue::TimerQueue(EventLoop* loop_) : own_loop(loop_), timerfd(createTimerfd())
{
    timerChannel = loop_->add_channel(timerfd);
    timerChannel->set_read_callback([this] { handleRead(); });
    timerChannel->EnableRead();
}

// Timer* should be managed by who create them
TimerQueue::~TimerQueue() { LOG_DEBUG << " "; }

void TimerQueue::addTimer(triggerTime_t _triggerTime, TimerCallback _cb, double _repeatCircle)
{
    LOG_DEBUG << " ";
    own_loop->assertInLoopThread();
    Timer* timer = new Timer(_triggerTime, _cb, _repeatCircle);
    own_loop->runInLoop([&] { addTimerInLoop(timer, 0); });
}

void TimerQueue::addTimer(Timer* _timer)
{
    LOG_DEBUG << " ";
    own_loop->assertInLoopThread();

    own_loop->runInLoop([&] { addTimerInLoop(_timer, 0); });
}

void TimerQueue::cancelTimer(Timer* _timer, bool _noOwner)
{
    LOG_DEBUG << " ";
    own_loop->assertInLoopThread();

    own_loop->runInLoop([&] { cancelTimerInLoop(_timer, _noOwner); });
}

void TimerQueue::addTimerInLoop(Timer* _timer, bool _noOwner)
{
    LOG_DEBUG << " ";
    own_loop->assertInLoopThread();

    // update timerfd
    if (insert(_timer, _noOwner))
    {
        updateFd(_timer->TriggerTime());
    }
}

void TimerQueue::cancelTimerInLoop(Timer* timer_, bool noOwner)
{
    LOG_DEBUG << " ";
    own_loop->assertInLoopThread();
    triggerTime_t t = timer_->TriggerTime();
    if (activeTimers.find(t) != activeTimers.end())
    {
        activeTimers[t].erase(timer_);
        if (noOwner) delete timer_;
        if (activeTimers[t].empty()) activeTimers.erase(t);
    }
    else
    {
        LOG_ERROR << " ";
    }
}

void TimerQueue::handleRead()
{
    LOG_DEBUG << " ";
    own_loop->assertInLoopThread();

    triggeredTimers.clear();
    triggerTime_t now = Timestamp::now_microsecconds();
    for (; activeTimers.begin()->first <= now;)
    {
        sameTime_timers_t ts = activeTimers.begin()->second;
        for (auto it : ts)
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
        if (activeTimers.empty())
        {
            activeTimers.clear();
            break;
        }
    }
    reset(now);
}

bool TimerQueue::insert(Timer* timer_, bool noOwner)
{
    LOG_DEBUG << " ";
    triggerTime_t triggerTime_ = timer_->TriggerTime();
    if (activeTimers.find(triggerTime_) != activeTimers.end())
    {
        LOG_DEBUG << " old triggerTime";
        sameTime_timers_t sameTime_timers = activeTimers[triggerTime_];
        if (sameTime_timers.find(timer_) != sameTime_timers.end())
        {
            LOG_ERROR << " ";
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
        LOG_DEBUG << " new triggerTime";
        activeTimers[timer_->TriggerTime()][timer_] = noOwner;
        // log success
        return activeTimers.begin()->first >= triggerTime_;
    }
}

void TimerQueue::reset(triggerTime_t now)
{
    LOG_DEBUG << " ";
    triggerTime_t earliest = 0xffffffffffffffff;
    for (Timer* it : triggeredTimers)
    {
        it->reset(now);
        addTimer(it);
        earliest = std::min(earliest, it->TriggerTime());
    }
    if (earliest != 0) updateFd(earliest);
}

void TimerQueue::updateFd(triggerTime_t _triggerTime)
{
    LOG_DEBUG << " _triggerTime=" << _triggerTime;
    struct itimerspec newtime, oldtime;
    memset(&newtime, 0, sizeof(newtime));
    memset(&oldtime, 0, sizeof(oldtime));
    periodFromNow(newtime, _triggerTime);
    if (::timerfd_settime(timerfd, 0, &newtime, &oldtime) < 0)
    {
        LOG_ERROR << "::timerfd_settime";
    }
}