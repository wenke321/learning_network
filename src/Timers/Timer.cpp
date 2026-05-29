#include "Timer.h"

#include "Timestamp.h"

void Timer::reset(triggerTime_t now)
{
    if (repeat)
    {
        triggerTime = addTime(now, repeatCircle);
    }
    else
    {
        triggerTime = 0;
    }
}
