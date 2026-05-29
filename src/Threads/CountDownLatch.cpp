#include "CountDownLatch.h"

CountDownLatch::CountDownLatch(int count_) : cond(mutex), count(count_) {}

void CountDownLatch::wait()
{
    MutexLockGuard lock(mutex);
    while (count > 0)
    {
        cond.wait();
    }
}

void CountDownLatch::countDown()
{
    MutexLockGuard lock(mutex);
    if (--count == 0)
    {
        cond.notifyAll();
    }
}

int CountDownLatch::get_count()
{
    MutexLockGuard lock(mutex);
    return count;
}