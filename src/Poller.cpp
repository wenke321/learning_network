
#include "Poller.h"

#include "Channel.h"
#include "EventLoop.h"

#define InitEventNum 1000

Poller::Poller(EventLoop* loop_) : owner_loop(loop_) {}
Poller::~Poller() {}

bool Poller::hasChannal(Channel* ch)
{
    owner_loop->assertInLoopThread();
    if (m_channels.find(ch->fd_()) != m_channels.end()) return true;
    return false;
}
