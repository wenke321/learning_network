
#pragma once

#include <unordered_map>
#include <vector>

#include "Channel.h"
#include "Mutex.h"
#include "Timestamp.h"

class EventLoop;

class Poller
{
   public:
    Poller(EventLoop* loop);
    virtual ~Poller();

    virtual Timestamp Poll(int timeout, std::vector<Channel*>& activeChannels_) = 0;
    Channel* add_channel(int fd);
    virtual void updateChannel(Channel* ch) = 0;
    void removeChannel(Channel* ch);

    bool hasChannal(Channel*);

    void assertInLoopThread();

   private:
    friend class Epoller;

    std::unordered_map<int, Channel*> m_channels;
    MutexLock mutex;
    EventLoop* owner_loop;
};

class Epoller : public Poller
{
   public:
    explicit Epoller(EventLoop* loop);
    ~Epoller();

    Timestamp Poll(int timeout, std::vector<Channel*>& activeChannals_) override;

    virtual void updateChannel(Channel*) override;
    // virtual void removeChannel(Channel*) override;
    void fillActiveChannels(int nfds, std::vector<Channel*>& activeChannals_);

   private:
    static const int initEventNum = 16;
    void update(int op, Channel*);

    std::vector<struct epoll_event> events;
    int epfd;
};
