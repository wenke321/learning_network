
#pragma once
#include "Condition.h"

class CountDownLatch
{
   public:
    explicit CountDownLatch(int);

    void wait();
    void countDown();
    int get_count();

   private:
    mutable MutexLock mutex;
    Condition cond;
    int count;
};