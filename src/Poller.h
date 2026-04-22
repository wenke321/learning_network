
#pragma once

#include <unordered_map>
#include <vector>

#include "Channel.h"
#include "EventLoop.h"
#include "Timestamp.h"

class Poller
{
   public:
    Poller(EventLoop* loop);
    virtual ~Poller();

    virtual Timestamp Poll(int timeout, std::vector<Channel*>& activeChannels_) = 0;
    virtual void updateChannel(Channel* ch)                                     = 0;
    virtual void removeChannel(Channel* ch)                                     = 0;

    virtual bool hasChannal(Channel*);

    void assertInLoopThread() { owner_loop->assertInLoopThread(); }

   protected:
    std::unordered_map<int, Channel*> m_channels;

   private:
    EventLoop* owner_loop;
};
