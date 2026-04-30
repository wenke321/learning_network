#pragma once
#include <map>
#include <memory>
#include <unordered_set>

#include "Channel.h"
#include "EventLoop.h"
#include "Timer.h"
#include "Timestamp.h"
class TimerQueue
{
   public:
    explicit TimerQueue(EventLoop* own_loop_);
    ~TimerQueue();

    void addTimer(triggerTime_t triggerTime_, TimerCallback cb, double repeatCircle_);
    void addTimer(Timer*);
    void cancelTimer(Timer*, bool);

   private:
    void addTimerInLoop(Timer*, bool);
    void cancelTimerInLoop(Timer*, bool);
    void handle_timers();
    bool insert(Timer*, bool);
    void updateFd(triggerTime_t);
    void reset(triggerTime_t);

    EventLoop* own_loop;
    int timerfd;
    Channel* timerChannel;
    std::vector<Timer*> triggeredTimers;
    std::map<triggerTime_t, sameTime_timers_t> activeTimers;
};
