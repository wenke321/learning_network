#pragma once

#include <sys/epoll.h>

#include <vector>

#include "Poller.h"
#include "Timer.h"
#include "Timestamp.h"

class Epoller : public Poller
{
   public:
    explicit Epoller(EventLoop* loop);
    ~Epoller();

    Timestamp Poll(int timeout, std::vector<Channel*>& activeChannals_) override;
    void updateChannel(Channel*) override;
    void removeChannel(Channel*) override;
    void fillActiveChannels(int nfds, std::vector<Channel*>& activeChannals_);

   private:
    static const int initEventNum = 16;
    void update(int op, Channel*);

    int epfd;
    std::vector<struct epoll_event> events;
};