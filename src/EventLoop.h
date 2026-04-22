
#pragma once
#include <fcntl.h>
#include <pthread.h>

#include <cstdlib>
#include <functional>
#include <memory>
#include <vector>

#include "Acceptor.h"
#include "CountDownLatch.h"
#include "Timer.h"
#include "Timestamp.h"

class Channel;
class TimerQueue;
class Epoller;

class EventLoop
{
   public:
    typedef std::function<void()> Functor;

    EventLoop();
    ~EventLoop();

    void Loop();
    void quit_();

    void runInLoop(Functor func);
    void queueInLoop(Functor func);
    void addTimer(Timer*);
    void cancelTimer(Timer*);
    void runAt(triggerTime_t triggerTime_, TimerCallback cb);
    void runAfter(triggerTime_t after, TimerCallback cb);
    void runEvery(triggerTime_t triggerTime_, TimerCallback cb, double repeatCircle_);

    void wakeup();
    void updateChannel(Channel* ch);
    void removeChannel(Channel* ch);
    bool hasChannal(Channel* ch);

    bool isInLoopThread() { return pthread_equal(pthread_self(), thread_id); }
    void abortNotInLoopThread() { abort(); }
    void assertInLoopThread()
    {
        if (!isInLoopThread()) abortNotInLoopThread();
    }

   private:
    void handleRead();  // wakeup
    void doPendingFunctors();

    bool quit;
    bool looping;
    bool eventHandling;
    bool callingPendingFunctors;
    int wakeup_fd;
    const pthread_t thread_id;
    std::unique_ptr<Epoller> epoller;
    std::unique_ptr<Channel> wakeupChannel;
    std::vector<Channel*> activeChannels;
    Channel* cur_activeCh;
    Timestamp pollReturnTime;
    std::unique_ptr<TimerQueue> timerQueue;

    MutexLock mutex;
    std::vector<Functor> pendingFunctors;
};
