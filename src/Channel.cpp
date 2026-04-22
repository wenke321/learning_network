
#include "Channel.h"

#include <sys/poll.h>
#include <unistd.h>

#include <cassert>
#include <csignal>
#include <memory>
#include <utility>

#include "EventLoop.h"
#include "Logger.h"
#include "Timestamp.h"

const int Channel::READ_EVENT  = POLL_IN | POLL_PRI;
const int Channel::WRITE_EVENT = POLL_OUT;
const int Channel::NONE_EVENT  = 0;

const int Channel::ch_new     = 1;
const int Channel::ch_added   = 2;
const int Channel::ch_deleted = 4;

Channel::Channel(int fd_, EventLoop* loop_) : fd(fd_), index(ch_new), loop(loop_), listen_events(0), ready_events(0), addedToLoop(false), eventHandling(false), tied(false) {}

Channel::~Channel()
{
    assert(!eventHandling);
    assert(!addedToLoop);
    if (loop->isInLoopThread()) assert(!loop->hasChannal(this));
}

EventLoop* Channel::owner_loop() { return loop; }

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
    }
    else
    {
        HandleEvent_tied();
    }
}

void Channel::HandleEvent_tied()
{
    eventHandling = true;
    if (ready_events & POLLHUP && !(ready_events & POLLIN))
    {
        LOG_INFO << "channel close,fd=" << fd;
        if (close_callback) close_callback();
    }

    if (ready_events & POLLNVAL)
    {
        LOG_ERROR << "channel invalid,fd=" << fd;
    }

    if (ready_events & (POLLERR | POLLNVAL))
    {
        if (error_callback) error_callback();
    }

    if (ready_events & (POLLIN | POLL_PRI | POLLRDHUP))
    {
        if (read_callback) read_callback();
    }

    if (ready_events & POLLOUT)
    {
        if (write_callback) write_callback();
    }
}

void Channel::EnableRead()
{
    LOG_INFO << "Channel::EnableRead,fd=" << fd;
    listen_events |= READ_EVENT;
    loop->updateChannel(this);
}

void Channel::EnableWrite()
{
    LOG_INFO << "Channel::EnableWrite,fd=" << fd;
    listen_events |= WRITE_EVENT;
    loop->updateChannel(this);
}

void Channel::DisableRead()
{
    LOG_INFO << "Channel::DisableRead,fd=" << fd;
    listen_events &= ~READ_EVENT;
    update();
}

void Channel::DisableWrite()
{
    LOG_INFO << "Channel::DisableWrite,fd=" << fd;
    listen_events &= ~WRITE_EVENT;
    update();
}

void Channel::DisableAll()
{
    LOG_INFO << "Channel::DisableAll,fd=" << fd;
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
    assert(isNoneEvent());
    addedToLoop = false;
    LOG_INFO << "Channel::remove,fd=" << fd;
    loop->removeChannel(this);
}

int Channel::fd_() const { return fd; }

int Channel::index_() { return index; }

int Channel::listen_events_() const { return listen_events; }
int Channel::ready_events_() const { return ready_events; }
bool Channel::isNoneEvent() const { return listen_events == NONE_EVENT; }

void Channel::set_ready_event(int ev)
{
    ready_events = ev;
    LOG_INFO << "Channel::set_ready_event,fd=" << fd << ",event=" << ev;
}

void Channel::tie_(const std::shared_ptr<void>& obj)
{
    LOG_INFO << "tie Tcpconnection,for safety,fd=" << fd;
    tiedObj = obj;
    tied    = true;
}

void Channel::set_read_callback(std::function<void()> callback)
{
    LOG_INFO << "Channel::set_read_callback,fd=" << fd;
    read_callback = std::move(callback);
}
void Channel::set_write_callback(std::function<void()> callback)
{
    LOG_INFO << "Channel::set_write_callback,fd=" << fd;
    write_callback = std::move(callback);
}
void Channel::set_error_callback(std::function<void()> callback)
{
    LOG_INFO << "Channel::set_erroe_callback,fd=" << fd;
    error_callback = std::move(callback);
}
void Channel::set_close_callback(std::function<void()> callback)
{
    LOG_INFO << "Channel::set_close_callback,fd=" << fd;
    close_callback = std::move(callback);
}
