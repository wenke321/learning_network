
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
        int sockfd = removeAndResetChannel();
        retry(sockfd);
    }
}

void Connector::connect()
{
    int sockfd = sockOption::createNonblockingOrDie(serverAddr_.family());

    int ret        = sockOption::connect(sockfd, serverAddr_.getSockAddr());
    int savedErrno = (ret == 0) ? 0 : errno;
    LOG_INFO << "Connector::connect";
    switch (savedErrno)
    {
        case 0:
        case EINPROGRESS:
            LOG_INFO << "EINPROGRESS,may succeed";
        case EINTR:
        case EISCONN:
            connecting(sockfd);
            break;

        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH:
            retry(sockfd);
            break;

        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
            LOG_SYSERR << "connect error in Connector::startInLoop " << savedErrno;
            sockOption::close(sockfd);
            break;

        default:
            LOG_SYSERR << "Unexpected error in Connector::startInLoop " << savedErrno;
            sockOption::close(sockfd);
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

bool Connector::checkConnect(int sock)
{
    LOG_INFO << "Connector::checkConnect,use select";
    // 使用 select 等待可写
    fd_set wset;
    FD_ZERO(&wset);
    FD_SET(sock, &wset);

    struct timeval tv = {1, 0};
    int ret           = ::select(sock + 1, NULL, &wset, NULL, &tv);
    if (ret <= 0)
    {
        // 超时或出错
        LOG_ERROR << "select failed! fd=" << sock;
        return 0;
    }

    // 检查 socket 错误状态
    int error     = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
    {
        LOG_ERROR << "getsockopt failed";
        return 0;
    }

    if (error != 0)
    {
        LOG_ERROR << "nonblock connect,Connector::checkConnect failed! fd=" << sock;
        return 0;
    }
    LOG_INFO << "Connector::checkConnect succeed,fd=" << sock;
    return true;
}

void Connector::connecting(int sockfd)
{
    if (!checkConnect(sockfd)) retry(sockfd);
    setState(Connecting);
    assert(!channel_);
    channel_.reset(new Channel(sockfd, loop_));
    channel_->set_write_callback([this] { handleWrite(); });  // FIXME: unsafe
    channel_->set_error_callback([this] { handleError(); });  // FIXME: unsafe

    channel_->EnableWrite();
}

int Connector::removeAndResetChannel()
{
    channel_->DisableAll();
    channel_->remove();
    int sockfd = channel_->fd_();
    // Can't reset channel_ here, because we are inside Channel::handleEvent
    loop_->queueInLoop([this] { resetChannel(); });  // FIXME: unsafe
    return sockfd;
}

void Connector::resetChannel() { channel_.reset(); }

void Connector::handleWrite()
{
    LOG_TRACE << "Connector::handleWrite " << state_;

    if (state_ == Connecting)
    {
        int sockfd = removeAndResetChannel();
        int err    = sockOption::getSocketError(sockfd);
        if (err)
        {
            LOG_WARN << "Connector::handleWrite - SO_ERROR = " << err << " " << strerrorInfo(err);
            retry(sockfd);
        }
        else if (sockOption::isSelfConnect(sockfd))
        {
            LOG_WARN << "Connector::handleWrite - Self connect";
            retry(sockfd);
        }
        else
        {
            LOG_INFO << "connect established";
            setState(Connected);
            if (connect_)
            {
                newConnectionCallback_(sockfd);
            }
            else
            {
                LOG_ERROR << "bool connect_=0,close fd=" << sockfd;
                sockOption::close(sockfd);
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
    LOG_ERROR << "Connector::handleError state=" << state_;
    if (state_ == Connecting)
    {
        int sockfd = removeAndResetChannel();
        int err    = sockOption::getSocketError(sockfd);
        LOG_TRACE << "SO_ERROR = " << err << " " << strerrorInfo(err);
        retry(sockfd);
    }
}

void Connector::retry(int sockfd)
{
    LOG_INFO << "retry connect,fd=" << sockfd;
    sockOption::close(sockfd);
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
