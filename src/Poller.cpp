
#include "Poller.h"

#include <sys/epoll.h>
#include <unistd.h>

#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#define InitEventNum 1000

Poller::Poller(EventLoop* loop_) : owner_loop(loop_) {}
Poller::~Poller()
{
    for (auto it : m_channels)
    {
        // it.second->DisableAll();
        delete it.second;
    }
}

bool Poller::hasChannal(Channel* ch)
{
    owner_loop->assertInLoopThread();
    if (m_channels.find(ch->fd_()) != m_channels.end()) return true;
    return false;
}

Channel* Poller::add_channel(int fd)
{
    LOG_DEBUG << " fd=" << fd;
    MutexLockGuard lock(mutex);
    if (m_channels.find(fd) == m_channels.end())
    {
        m_channels[fd] = new Channel(fd, owner_loop);
        m_channels[fd]->setIndex(Channel::ch_deleted);
        return m_channels[fd];
    }
    else
    {
        m_channels[fd]->reset_listen_events();
        return m_channels[fd];
    }
}

void Poller::removeChannel(Channel* ch)
{
    LOG_INFO << "Epoller::removeChannel,fd=" << ch->fd_();
    Poller::assertInLoopThread();

    int fd = ch->fd_();
    assert(m_channels.find(fd) != m_channels.end());

    delete ch;

    m_channels.erase(fd);
}

void Poller::assertInLoopThread() { owner_loop->assertInLoopThread(); }

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
    LOG_TRACE << "Epoller::updateChannel,fd=" << ch->fd_();
    Poller::assertInLoopThread();

    const int idx = ch->index_();
    if (idx & Channel::ch_deleted)
    {
        // LOG_DEBUG << " idx=Channel::ch_deleted";
        int fd = ch->fd_();

        assert(m_channels.find(fd) != m_channels.end());
        assert(m_channels[fd] == ch);

        ch->setIndex(Channel::ch_added);
        update(EPOLL_CTL_ADD, ch);
    }
    else if (idx & Channel::ch_extern)
    {
        LOG_DEBUG << " extern Channel";
        int fd = ch->fd_();

        assert(m_channels.find(fd) == m_channels.end());
        m_channels[fd] = ch;

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
        }
        else
            update(EPOLL_CTL_MOD, ch);
    }
}

void Epoller::update(int op, Channel* ch)
{
    LOG_TRACE << "Epoller::update";
    assertInLoopThread();
    int fd = ch->fd_();
    struct epoll_event ev;
    ev.events   = ch->listen_events_() | EPOLLET;
    ev.data.ptr = ch;
    if (::epoll_ctl(epfd, op, fd, &ev) < 0)
    {
        LOG_ERROR << "epoll_ctl error,fd=" << fd;
    }
}
