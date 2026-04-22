#include "Epoller.h"

#include <sys/epoll.h>
#include <unistd.h>

#include <cassert>
#include <cstddef>

#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Timestamp.h"

Epoller::Epoller(EventLoop* loop_) : Poller(loop_), epfd(::epoll_create1(EPOLL_CLOEXEC)), events(initEventNum)
{
    if (epfd < 0)
    {
        LOG_ERROR << "::epoll_create1 failed!!!";
    }
}

Epoller::~Epoller() { ::close(epfd); }

Timestamp Epoller::Poll(int timeout, std::vector<Channel*>& activeChannels_)
{
    int nfds = epoll_wait(epfd, &*events.begin(), static_cast<int>(events.size()), timeout);
    int err  = errno;

    Timestamp now = Timestamp::now_microsecconds();
    if (nfds > 0)
    {
        fillActiveChannels(nfds, activeChannels_);
        if ((size_t)nfds == events.size()) events.resize(events.size() * 2);
    }
    else if (nfds == 0)
    {
        LOG_INFO << "Epoller::Poll no event";
    }
    else
    {
        if (err != EINTR)
        {
            LOG_ERROR << "Epoller::Poll error";
        }
    }

    return now;
}

void Epoller::fillActiveChannels(int nfds, std::vector<Channel*>& activeChannals_)
{
    for (int i = 0; i < nfds; i++)
    {
        Channel* ch = static_cast<Channel*>(events[i].data.ptr);

        ch->set_ready_event(events[i].events);
        activeChannals_.push_back(ch);
    }
}

void Epoller::updateChannel(Channel* ch)
{
    LOG_DEBUG << "Epoller::updateChannel,fd=" << ch->fd_();
    Poller::assertInLoopThread();

    const int idx = ch->index_();
    if (idx & (Channel::ch_new | Channel::ch_deleted))
    {
        int fd = ch->fd_();
        if (idx & Channel::ch_new)
        {
            assert(m_channels.find(fd) == m_channels.end());
            m_channels[fd] = ch;
        }
        else
        {
            assert(m_channels.find(fd) != m_channels.end());
            assert(m_channels[fd] == ch);
        }
        ch->setIndex(Channel::ch_added);
        update(EPOLL_CTL_ADD, ch);
    }
    else
    {  // added
        int fd = ch->fd_();
        assert(m_channels.find(fd) != m_channels.end());
        assert(m_channels[fd] == ch);

        if (ch->listen_events_() == 0)
        {
            ch->setIndex(Channel::ch_deleted);
            update(EPOLL_CTL_DEL, ch);
            m_channels.erase(ch->fd_());
        }
        else
            update(EPOLL_CTL_MOD, ch);
    }
}

void Epoller::removeChannel(Channel* ch)
{
    LOG_INFO << "Epoller::removeChannel,fd=" << ch->fd_();
    Poller::assertInLoopThread();

    int fd = ch->fd_();
    assert(m_channels.find(fd) != m_channels.end());

    m_channels.erase(fd);
    update(EPOLL_CTL_DEL, ch);
}

void Epoller::update(int op, Channel* ch)
{
    LOG_DEBUG << "Epoller::update";
    Poller::assertInLoopThread();
    int fd = ch->fd_();
    struct epoll_event ev;
    ev.events   = ch->listen_events_() | EPOLLET;
    ev.data.ptr = ch;
    if (::epoll_ctl(epfd, op, fd, &ev) < 0)
    {
        LOG_ERROR << "epoll_ctl error";
    }
}