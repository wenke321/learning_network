
#include "Channel.h"

#include <sys/epoll.h>
#include <sys/poll.h>
#include <unistd.h>

#include <cassert>
#include <csignal>
#include <memory>
#include <utility>

#include "EventLoop.h"
#include "Logger.h"
#include "Timestamp.h"

const int Channel::READ_EVENT  = EPOLLIN | EPOLLPRI;
const int Channel::WRITE_EVENT = EPOLLOUT;
const int Channel::NONE_EVENT  = 0;

const int Channel::ch_new     = 1;
const int Channel::ch_added   = 2;
const int Channel::ch_deleted = 4;

Channel::Channel(int fd_, EventLoop* loop_) : fd(fd_), idx(ch_new), loop(loop_), listen_events(0), ready_events(0), addedToLoop(false), eventHandling(false), tied(false) {}

Channel::~Channel()
{
    assert(!eventHandling);
    assert(!addedToLoop);
    if (loop->isInLoopThread()) assert(!loop->hasChannal(this));
}

EventLoop* Channel::owner_loop() { return loop; }
void Channel::setIndex(int _idx) { idx = _idx; }
bool Channel::isWriting() { return listen_events & WRITE_EVENT; }
bool Channel::isReading() { return listen_events & READ_EVENT; }

void Channel::HandleEvent()
{
    std::shared_ptr<void> guard;
    if (tied)
    {
        guard = tiedObj.lock();
        if (guard)
        {
            HandleEvent_tied();
        }
        else
        {
            LOG_WARN << " Tcpconnection break already,fd=" << fd;
        }
    }
    else
    {
        HandleEvent_tied();
    }
}

void Channel::HandleEvent_tied()
{
    LOG_TRACE << " ";
    eventHandling = true;
    if (ready_events & EPOLLERR)
    {
        LOG_TRACE << " EPOLLERR";
        if (error_callback) error_callback();
    }
    if (ready_events & EPOLLOUT)
    {
        LOG_TRACE << " EPOLLOUT";
        if (write_callback) write_callback();
    }

    // OOB msg
    if (ready_events & EPOLLPRI)
    {
        LOG_TRACE << " EPOLLPRI";
        if (read_callback) read_callback();
    }

    if (ready_events & EPOLLIN)
    {
        LOG_TRACE << " EPOLLIN";
        if (read_callback) read_callback();
    }
    if (ready_events == (EPOLLHUP | EPOLLIN))
    {
        LOG_TRACE << " EPOLLHUP|EPOLLIN";
        // finish recv and close
    }

    // receive FIN,or he SHUT_WR/close
    if (ready_events & EPOLLRDHUP)
    {
        LOG_TRACE << " EPOLLRDHUP";
        //
    }

    if (ready_events & EPOLLHUP)
    {
        LOG_TRACE << " EPOLLHUP";
        LOG_INFO << "channel close,fd=" << fd;
        if (close_callback) close_callback();
    }

    // if (ready_events & POLLNVAL)
    // {
    //     LOG_ERROR << "channel invalid,fd=" << fd;
    // }

    eventHandling = false;
}

void Channel::EnableRead()
{
    LOG_TRACE << " fd=" << fd;
    listen_events |= READ_EVENT;
    loop->updateChannel(this);
}

void Channel::EnableWrite()
{
    LOG_TRACE << " fd=" << fd;
    listen_events |= WRITE_EVENT;
    loop->updateChannel(this);
}

void Channel::DisableRead()
{
    LOG_DEBUG << "fd=" << fd;
    listen_events &= ~READ_EVENT;
    update();
}

void Channel::DisableWrite()
{
    LOG_DEBUG << " fd=" << fd;
    listen_events &= ~WRITE_EVENT;
    update();
}

void Channel::DisableAll()
{
    LOG_DEBUG << " fd=" << fd;
    listen_events = 0;
    loop->updateChannel(this);
}

void Channel::update()
{
    addedToLoop = true;
    loop->updateChannel(this);
}

void Channel::remove()
{
    LOG_TRACE << " fd=" << fd;
    assert(isNoneEvent());
    addedToLoop = false;
    loop->removeChannel(this);
}

int Channel::fd_() const { return fd; }

int Channel::index_() { return idx; }

int Channel::listen_events_() const { return listen_events; }
int Channel::ready_events_() const { return ready_events; }
bool Channel::isNoneEvent() const { return listen_events == NONE_EVENT; }

void Channel::reset_listen_events() { listen_events = NONE_EVENT; }

void Channel::set_ready_event(int ev)
{
    LOG_TRACE << " fd=" << fd << ",event=" << ev;
    ready_events = ev;
}

void Channel::tie_(const std::shared_ptr<void>& obj)
{
    LOG_TRACE << " fd=" << fd;
    tiedObj = obj;
    tied    = true;
}

void Channel::set_read_callback(std::function<void()> callback)
{
    LOG_TRACE << " fd=" << fd;
    read_callback = std::move(callback);
    // if (!read_callback) LOG_DEBUG << " failed";
}
void Channel::set_write_callback(std::function<void()> callback)
{
    LOG_TRACE << " fd=" << fd;
    write_callback = std::move(callback);
}
void Channel::set_error_callback(std::function<void()> callback)
{
    LOG_TRACE << " fd=" << fd;
    error_callback = std::move(callback);
}
void Channel::set_close_callback(std::function<void()> callback)
{
    LOG_TRACE << " fd=" << fd;
    close_callback = std::move(callback);
}
