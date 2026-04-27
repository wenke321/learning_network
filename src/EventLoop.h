
#pragma once
#include <fcntl.h>
#include <pthread.h>

#include <cstdlib>
#include <functional>
#include <memory>
#include <vector>

#include "Acceptor.h"
#include "Poller.h"
#include "Timer.h"
#include "Timestamp.h"

class Channel;
class TimerQueue;

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
    void runAfter(double after, TimerCallback cb);
    void runEvery(TimerCallback cb, double repeatCircle_);

    void wakeup();
    Channel* add_channel(int fd);
    void add_channel(Channel*);
    void updateChannel(Channel* ch);
    void removeChannel(Channel* ch);
    bool hasChannal(Channel* ch);

    bool isInLoopThread();
    void abortNotInLoopThread();
    void assertInLoopThread();

   private:
    void handleRead();  // wakeup
    void doPendingFunctors();

    bool quit;
    bool looping;
    bool eventHandling;
    bool callingPendingFunctors;
    int wakeup_fd;
    const pid_t tid_;
    std::unique_ptr<Poller> epoller;
    Channel* wakeupChannel;
    std::vector<Channel*> activeChannels;
    Channel* cur_activeCh;
    Timestamp pollReturnTime;
    std::unique_ptr<TimerQueue> timerQueue;

    MutexLock mutex;
    std::vector<Functor> pendingFunctors;
};
