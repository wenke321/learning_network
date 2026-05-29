#pragma once

#include <cstdint>
#include <string>

#include "Timer.h"

class Timestamp
{
   public:
    Timestamp(uint64_t microSecondsSinceEpoch_) : _microSecondsSinceEpoch(microSecondsSinceEpoch_) {}
    Timestamp() : _microSecondsSinceEpoch(0) {}

    uint64_t microSecondsSinceEpoch() { return _microSecondsSinceEpoch; }
    std::string toStr();
    std::string toStrYMD();

    static Timestamp invalid() { return Timestamp(); }
    static Timestamp now();
    static uint64_t now_microsecconds();
    bool valid() const { return _microSecondsSinceEpoch > 0; }

    inline bool operator==(Timestamp& other) { return other.microSecondsSinceEpoch() == _microSecondsSinceEpoch; }
    inline bool operator<(Timestamp& other) { return other.microSecondsSinceEpoch() > _microSecondsSinceEpoch; }

   private:
    uint64_t _microSecondsSinceEpoch;
};
inline Timestamp add(Timestamp now, double second)
{
    uint64_t d = static_cast<uint64_t>(second * 1000000);
    return Timestamp(now.microSecondsSinceEpoch() + d);
}

inline double timeDifference(Timestamp high, Timestamp low)
{
    int64_t diff = high.microSecondsSinceEpoch() - low.microSecondsSinceEpoch();
    return static_cast<double>(diff) / 1000000;
}

inline triggerTime_t addTime(triggerTime_t now, double second) { return static_cast<uint64_t>(second * 1000000) + now; }