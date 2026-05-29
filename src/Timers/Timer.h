#pragma once
#include <fcntl.h>

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

typedef std::function<void()> TimerCallback;
typedef uint64_t triggerTime_t;

class Timer
{
   public:
    // when,repeatCircle:microseconds
    explicit Timer(triggerTime_t when, TimerCallback cb_, double repeatCircle_ = 0) : triggerTime(when), cb(std::move(cb_)), repeatCircle(repeatCircle_), repeat(repeatCircle > 0.0) {}
    void run() { cb(); }
    triggerTime_t TriggerTime() { return triggerTime; }
    double repeatCircle_() { return repeatCircle; }
    bool shouldRepeat() { return repeat; }
    // uint64_t Sequence() { return sequence; }

    void reset(triggerTime_t);

    // static uint64_t TimerNum() { return __atomic_load_n(&createdNum, __ATOMIC_SEQ_CST); }

   private:
    triggerTime_t triggerTime;
    TimerCallback cb;
    const double repeatCircle;
    const bool repeat;
    // const uint64_t sequence;
    //  static uint64_t createdNum;
};

typedef std::unordered_map<Timer*, bool> sameTime_timers_t;