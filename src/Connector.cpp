
#include "Connector.h"

#include <errno.h>

#include <cassert>

#include "EventLoop.h"
#include "Logger.h"

const int Connector::MaxRetryDelayMs;

Connector::Connector(EventLoop* loop, const InetAddr& serverAddr) : loop_(loop), serverAddr_(serverAddr), connect_(false), state_(Disconnected), retryDelayMs_(InitRetryDelayMs) { LOG_TRACE << "ctor[" << this << "]"; }

Connector::~Connector()
{
    LOG_TRACE << "dtor[" << this << "]";
    assert(!channel_);
}

void Connector::start()
{
    connect_ = true;
    loop_->runInLoop([this] { startInLoop(); });  // FIXME: unsafe
}

void Connector::startInLoop()
{
    loop_->assertInLoopThread();
    assert(state_ == Disconnected);
    LOG_TRACE << "Connector::startInLoop";
    if (connect_)
    {
        connect();
    }
    else
    {
        // LOG_DEBUG << "do not connect";
    }
}

void Connector::stop()
{
    connect_ = false;
    loop_->queueInLoop(std::bind(&Connector::stopInLoop, this));  // FIXME: unsafe
                                                                  // FIXME: cancel timer
}

void Connector::stopInLoop()
{
    loop_->assertInLoopThread();
    if (state_ == Connecting)
    {
        setState(Disconnected);
        int serverfd = removeAndResetChannel();
        retry(serverfd);
    }
}

void Connector::connect()
{
    int serverfd = sockOption::createNonblockingOrDie(serverAddr_.family());

    int ret        = sockOption::connect(serverfd, serverAddr_.getSockAddr());
    int savedErrno = (ret == 0) ? 0 : errno;
    LOG_TRACE << "Connector::connect";
    switch (savedErrno)
    {
        case 0:
        case EINPROGRESS:
        case EINTR:
        case EISCONN:
            LOG_DEBUG << " expected errno,may succeed";
            connecting(serverfd);
            break;

        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH:
            retry(serverfd);
            break;

        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
            LOG_SYSERR << "connect error in Connector::startInLoop " << savedErrno;
            sockOption::close(serverfd);
            break;

        default:
            LOG_SYSERR << "Unexpected error in Connector::startInLoop " << savedErrno;
            sockOption::close(serverfd);
            break;
    }
}

void Connector::restart()
{
    loop_->assertInLoopThread();
    setState(Disconnected);
    retryDelayMs_ = InitRetryDelayMs;
    connect_      = true;
    startInLoop();
}

void Connector::connecting(int serverfd)
{
    LOG_DEBUG << " fd=" << serverfd;
    assert(!channel_);
    setState(Connecting);

    channel_ = loop_->add_channel(serverfd);
    channel_->set_out_callback([this] { check_whether_connected(); });
    channel_->set_err_callback([this] { handle_err(); });
    channel_->EnableWrite();
}

int Connector::removeAndResetChannel()
{
    LOG_DEBUG << " ";
    channel_->DisableAll();
    return channel_->fd_();
}

void Connector::resetChannel() { channel_ = nullptr; }

void Connector::check_whether_connected()
{
    LOG_TRACE << " Connector::check_whether_connected,state_=" << state_;

    if (state_ == Connecting)
    {
        // int serverfd = removeAndResetChannel();
        int fd  = channel_->fd_();
        int err = sockOption::getSocketError(fd);
        if (err)
        {
            LOG_WARN << "Connector::check_whether_connected - SO_ERROR = " << err << " " << strerrorInfo(err);
            retry(fd);
        }
        else if (sockOption::isSelfConnect(fd))
        {
            LOG_WARN << "Connector::check_whether_connected - Self connect";
            retry(fd);
        }
        else
        {
            LOG_DEBUG << " connect established,fd=" << fd;
            setState(Connected);
            channel_->reset_listen_events();
            if (connect_)
            {
                newConnectionCallback_(fd);
            }
            else
            {
                LOG_ERROR << "bool connect_=0,close fd=" << fd;
                sockOption::close(fd);
            }
        }
    }
    else
    {
        LOG_ERROR << "Connector::check_whether_connected,error don't know";
        assert(state_ == Disconnected);
    }
}

void Connector::handle_err()
{
    LOG_DEBUG << "Connector::handleError state=" << state_;
    if (state_ == Connecting)
    {
        int serverfd = removeAndResetChannel();
        int err      = sockOption::getSocketError(serverfd);
        LOG_TRACE << "SO_ERROR = " << err << " " << strerrorInfo(err);
        retry(serverfd);
    }
}

void Connector::retry(int serverfd)
{
    LOG_TRACE << "retry connect,fd=" << serverfd;
    sockOption::close(serverfd);
    setState(Disconnected);
    if (connect_)
    {
        LOG_INFO << "Connector::retry - Retry connecting to " << serverAddr_.ipPortStr() << " in " << retryDelayMs_ << " milliseconds. ";
        loop_->runAfter(retryDelayMs_ / 1000.0, [self = shared_from_this()] { self->startInLoop(); });
        retryDelayMs_ = std::min(retryDelayMs_ * 2, MaxRetryDelayMs);
    }
    else
    {
        // LOG_DEBUG << "do not connect";
    }
}
