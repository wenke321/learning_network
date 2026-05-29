#include <pthread.h>

#include <cstddef>

#include "Mutex.h"

class Condition
{
   public:
    explicit Condition(MutexLock& mutex_) : mutex(mutex_) { MCHECK(pthread_cond_init(&pcond, NULL)); }
    ~Condition() { MCHECK(pthread_cond_destroy(&pcond)); }
    void wait()
    {
        MutexLock::UnassignGuard ug(mutex);
        pthread_cond_wait(&pcond, mutex.getPthreadMutex());
    }
    void notify() { pthread_cond_signal(&pcond); }
    void notifyAll() { pthread_cond_broadcast(&pcond); }

   private:
    MutexLock& mutex;
    pthread_cond_t pcond;
};