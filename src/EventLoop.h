
#pragma once
#include <fcntl.h>
#include <pthread.h>

#include <cstdlib>
#include <functional>
#include <memory>
#include <vector>

#include "Acceptor.h"
#include "Poller.h"
#include "TcpConnection.h"
#include "Timer.h"
#include "Timestamp.h"
#include "helpers/queue.h"

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

    void runInLoop(Functor);
    void queueInLoop(Functor);

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
    void handle_wakeup();  // wakeup
    void doPendingFunctors();

    mpmc_bounded_Queue<Functor> pendingFunctors;
    std::vector<Channel*> activeChannels;
    std::unique_ptr<Poller> epoller;
    Channel* wakeupChannel;
    Channel* cur_activeCh;
    Timestamp pollReturnTime;
    std::unique_ptr<TimerQueue> timerQueue;
    const pid_t tid_;
    int wakeup_fd;
    bool quit;
    bool looping;
    bool eventHandling;
    bool callingPendingFunctors;
};
