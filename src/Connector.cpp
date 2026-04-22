
#include "Connector.h"

#include <errno.h>

#include <cassert>

#include "EventLoop.h"
#include "Logger.h"

const int Connector::MaxRetryDelayMs;

Connector::Connector(EventLoop* loop, const InetAddr& serverAddr) : loop_(loop), serverAddr_(serverAddr), connect_(false), state_(Disconnected), retryDelayMs_(InitRetryDelayMs) { LOG_DEBUG << "ctor[" << this << "]"; }

Connector::~Connector()
{
    LOG_DEBUG << "dtor[" << this << "]";
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
    LOG_INFO << "Connector::startInLoop";
    if (connect_)
    {
        connect();
    }
    else
    {
        LOG_DEBUG << "do not connect";
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
    LOG_INFO << "Connector::connect";
    switch (savedErrno)
    {
        case 0:
        case EINPROGRESS:
            LOG_INFO << "EINPROGRESS,may succeed";
        case EINTR:
        case EISCONN:
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
            // connectErrorCallback_();
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
    // if (!checkConnect(serverfd)) retry(serverfd);
    setState(Connecting);
    assert(!channel_);
    channel_.reset(new Channel(serverfd, loop_));
    channel_->set_write_callback([this] { handleWrite(); });  // FIXME: unsafe
    channel_->set_error_callback([this] { handleError(); });  // FIXME: unsafe

    channel_->EnableWrite();
}

int Connector::removeAndResetChannel()
{
    channel_->DisableAll();
    channel_->remove();
    int serverfd = channel_->fd_();
    // Can't reset channel_ here, because we are inside Channel::handleEvent
    loop_->queueInLoop([this] { resetChannel(); });  // FIXME: unsafe
    return serverfd;
}

void Connector::resetChannel() { channel_.reset(); }

void Connector::handleWrite()
{
    LOG_DEBUG << " Connector::handleWrite " << state_;

    if (state_ == Connecting)
    {
        // int serverfd = removeAndResetChannel();
        int fd  = channel_->fd_();
        int err = sockOption::getSocketError(fd);
        if (err)
        {
            LOG_WARN << "Connector::handleWrite - SO_ERROR = " << err << " " << strerrorInfo(err);
            retry(fd);
        }
        else if (sockOption::isSelfConnect(fd))
        {
            LOG_WARN << "Connector::handleWrite - Self connect";
            retry(fd);
        }
        else
        {
            LOG_DEBUG << "connect established";
            channel_->DisableAll();
            setState(Connected);
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
        LOG_ERROR << "Connector::handleWrite,error don't know";
        assert(state_ == Disconnected);
    }
}

void Connector::handleError()
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
    LOG_DEBUG << "retry connect,fd=" << serverfd;
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
        LOG_DEBUG << "do not connect";
    }
}
