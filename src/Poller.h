
#pragma once

#include <unordered_map>
#include <vector>

#include "Channel.h"
#include "Timestamp.h"

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

   protected:
    std::unordered_map<int, Channel*> m_channels;

   private:
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

    int epfd;
    std::vector<struct epoll_event> events;
};
