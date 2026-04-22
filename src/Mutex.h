#include <fcntl.h>
#include <pthread.h>

#include <cassert>
#include <cstddef>
#define MCHECK(ret)                   \
    ({                                \
        __typeof__(ret) errnum = ret; \
        assert(errnum == 0);          \
        (void)errnum;                 \
    })

#define selfTid pthread_self;

class MutexLock
{
   public:
    MutexLock() : holder(0) { MCHECK(pthread_mutex_init(&mutex, NULL)); }
    ~MutexLock()
    {
        assert(holder == 0);
        pthread_mutex_destroy(&mutex);
    }

    bool isLockedByThisThread() { return holder == pthread_self(); }

    void lock()
    {
        MCHECK(pthread_mutex_lock(&mutex));
        assignHolder();
    }

    void unlock()
    {
        unassignHolder();
        pthread_mutex_unlock(&mutex);
    }

    pthread_mutex_t* getPthreadMutex() { return &mutex; }

   private:
    friend class Condition;

    class UnassignGuard
    {
       public:
        UnassignGuard(MutexLock& owner_) : owner(owner_) { owner.unassignHolder(); }
        ~UnassignGuard() { owner.assignHolder(); }

       private:
        MutexLock& owner;
    };

    void assignHolder() { holder = pthread_self(); }
    void unassignHolder() { holder = 0; }

    pthread_t holder;
    pthread_mutex_t mutex;
};

class MutexLockGuard
{
   public:
    MutexLockGuard(MutexLock& mutex_) : mutex(mutex_) { mutex.lock(); }
    ~MutexLockGuard() { mutex.unlock(); }

   private:
    MutexLock& mutex;
};
