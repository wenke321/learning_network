
#include "TcpConnection.h"

#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstring>
#include <functional>
#include <memory>
#include <string>

#include "Buffer.h"
#include "Channel.h"
#include "Logger.h"
#include "Socket.h"
#include "Timer.h"
#include "Timestamp.h"
#include "WeakCallback.h"

void defaultConnectionCallback(const TcpConnectionPtr& conn)
{
    LOG_TRACE << conn->localAddress()->ipPortStr() << " -> " << conn->peerAddress()->ipPortStr() << " is " << (conn->connected() ? "UP" : "DOWN");
    // do not call conn->forceClose(), because some users want to register message callback only.
}

void defaultMessageCallback(const TcpConnectionPtr&, Buffer* buf) { buf->retrieveAll(); }

TcpConnection::TcpConnection(EventLoop* loop, const std::string& nameArg, int sockfd, const InetAddr* localAddr, const InetAddr* peerAddr) : state_(Connecting), name_(nameArg), reading_(true), socket_(new Socket(sockfd)), localAddr_(localAddr), peerAddr_(peerAddr), highWaterMark_(64 * 1024 * 1024), loop_(loop)
{
    channel_ = loop_->add_channel(sockfd);
    channel_->set_in_callback([this] { handle_ep_in(); });
    channel_->set_out_callback([this] { handle_ep_out(); });
    channel_->set_rdhup_callback([this] { handle_ep_rdhup(); });
    channel_->set_hup_callback([this] { handle_ep_hup(); });
    channel_->set_err_callback([this] { handle_ep_err(); });
    LOG_DEBUG << " TcpConnection::ctor[" << name_ << "] at " << this << " fd=" << sockfd;
    socket_->setKeepAlive(true);
}

TcpConnection::TcpConnection(Channel*& ch, const std::string& name, const InetAddr* localAddr, const InetAddr* peerAddr) : state_(Connecting), name_(name), reading_(true), localAddr_(localAddr), peerAddr_(peerAddr), highWaterMark_(64 * 1024 * 1024)
{
    channel_ = ch;
    loop_    = channel_->owner_loop();
    socket_  = std::make_unique<Socket>(channel_->fd_());

    channel_->set_in_callback([this] { handle_ep_in(); });
    channel_->set_out_callback([this] { handle_ep_out(); });
    channel_->set_rdhup_callback([this] { handle_ep_rdhup(); });
    channel_->set_hup_callback([this] { handle_ep_hup(); });
    channel_->set_err_callback([this] { handle_ep_err(); });
    LOG_DEBUG << " TcpConnection::ctor[" << name_ << "] at " << this << " fd=" << channel_->fd_();
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_DEBUG << " TcpConnection::dtor[" << name_ << "] at " << this << " fd=" << channel_->fd_() << " state=" << stateToString();
    assert(state_ == Disconnected);
}

EventLoop* TcpConnection::getLoop() const { return loop_; }
const std::string& TcpConnection::name() const { return name_; }

const InetAddr* TcpConnection::localAddress() const { return localAddr_; }
const InetAddr* TcpConnection::peerAddress() const { return peerAddr_; }

bool TcpConnection::connected() const { return state_ == Connected; }
bool TcpConnection::disconnected() const { return state_ == Disconnected; }

bool TcpConnection::getTcpInfo(struct tcp_info* tcpi) const { return socket_->getTcpInfo(tcpi); }

std::string TcpConnection::getTcpInfoString() const
{
    char buf[1024];
    buf[0] = '\0';
    socket_->getTcpInfoString(buf, sizeof(buf));
    return buf;
}

void TcpConnection::send(const void* data, int len) { send(stringPiece(static_cast<const char*>(data), len)); }

void TcpConnection::send(const stringPiece& message)
{
    LOG_DEBUG << " ";
    if (state_ == Connected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(message);
        }
        else
        {
            // void (TcpConnection::*fp)(const stringPiece& message) = &TcpConnection::sendInLoop;
            loop_->runInLoop([=] { sendInLoop(message); });
            // std::forward<string>(message)));
        }
    }
}

// FIXME efficiency!!!
void TcpConnection::send(Buffer* buf)
{
    if (state_ == Connected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(buf->readBegin(), buf->readableBytes());
            buf->retrieveAll();
        }
        else
        {
            // void (TcpConnection::*fp)(const stringPiece& message) = &TcpConnection::sendInLoop;
            std::string data = buf->retrieveAllAsString();
            loop_->runInLoop([=] { sendInLoop(data.data(), data.size()); });
            // std::forward<string>(message)));
        }
    }
}

void TcpConnection::sendInLoop(const stringPiece& message) { sendInLoop(message.data(), message.size()); }

void TcpConnection::sendInLoop(const void* data, size_t len)
{
    loop_->assertInLoopThread();
    ssize_t nwrote   = 0;
    size_t remaining = len;
    bool faultError  = false;
    if (state_ == Disconnected)
    {
        LOG_WARN << "disconnected, give up writing";
        return;
    }
    // if no thing in output queue, try writing directly
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = sockOption::write(channel_->fd_(), data, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop([this] { writeCompleteCallback_(shared_from_this()); });
            }
        }
        else  // nwrote < 0
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
                LOG_SYSERR << "TcpConnection::sendInLoop";
                if (errno == EPIPE || errno == ECONNRESET)  // FIXME: any others?
                {
                    faultError = true;
                }
            }
        }
    }

    assert(remaining <= len);
    if (!faultError && remaining > 0)
    {
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append(static_cast<const char*>(data) + nwrote, remaining);
        if (!channel_->isWriting())
        {
            channel_->EnableWrite();
        }
    }
}

void TcpConnection::shutdown()
{
    // FIXME: use compare and swap
    if (state_ == Connected)
    {
        LOG_INFO << "TcpConnection::shutdown,fd=" << channel_->fd_() << " local addr=" << localAddr_->ipPortStr();
        setState(Disconnecting);

        loop_->runInLoop([conn_ = shared_from_this()] { conn_->shutdownInLoop(); });
    }
}

void TcpConnection::shutdownInLoop()
{
    LOG_DEBUG << " ";
    loop_->assertInLoopThread();

    // we are not writing
    socket_->shutdownWrite();
}

// void TcpConnection::shutdownAndForceCloseAfter(double seconds)
// {
//   // FIXME: use compare and swap
//   if (state_ == kConnected)
//   {
//     setState(kDisconnecting);
//     loop_->runInLoop(std::bind(&TcpConnection::shutdownAndForceCloseInLoop, this, seconds));
//   }
// }

// void TcpConnection::shutdownAndForceCloseInLoop(double seconds)
// {
//   loop_->assertInLoopThread();
//   if (!channel_->isWriting())
//   {
//     // we are not writing
//     socket_->shutdownWrite();
//   }
//   loop_->runAfter(
//       seconds,
//       makeWeakCallback(shared_from_this(),
//                        &TcpConnection::forceCloseInLoop));
// }

void TcpConnection::forceClose()
{
    // FIXME: use compare and swap
    if (state_ == Connected || state_ == Disconnecting)
    {
        setState(Disconnecting);
        loop_->queueInLoop([conn = shared_from_this()] { conn->forceCloseInLoop(); });
    }
}

void TcpConnection::forceCloseWithDelay(double seconds)
{
    if (state_ == Connected || state_ == Disconnecting)
    {
        setState(Disconnecting);
        // loop_->runAfter(seconds, makeWeakCallback(shared_from_this(),
        //                                           &TcpConnection::forceClose));
        Timer* timer = new Timer(Timestamp::now_microsecconds() + seconds * 1000000, [conn = shared_from_this()] { conn->forceCloseInLoop(); }, 0);
        loop_->addTimer(timer);
    }
}

void TcpConnection::forceCloseInLoop()
{
    loop_->assertInLoopThread();
    if (state_ == Connected || state_ == Disconnecting)
    {
        // as if we received 0 byte in handleRead();
        handle_ep_hup();
    }
}

const char* TcpConnection::stateToString() const
{
    switch (state_)
    {
        case Disconnected:
            return "Disconnected";
        case Connecting:
            return "Connecting";
        case Connected:
            return "Connected";
        case Disconnecting:
            return "Disconnecting";
        default:
            return "unknown state";
    }
}

void TcpConnection::setTcpNoDelay(bool on) { socket_->setTcpNoDelay(on); }

void TcpConnection::startRead()
{
    loop_->runInLoop([this] { startReadInLoop(); });
}

void TcpConnection::startReadInLoop()
{
    loop_->assertInLoopThread();
    if (!channel_->isReading())
    {
        channel_->EnableRead();
    }
}

void TcpConnection::stopRead() { loop_->runInLoop(std::bind(&TcpConnection::stopReadInLoop, this)); }

void TcpConnection::stopReadInLoop()
{
    loop_->assertInLoopThread();
    if (reading_ || channel_->isReading())
    {
        channel_->DisableRead();
        reading_ = false;
    }
}

void TcpConnection::connectEstablished()
{
    LOG_DEBUG << " ";
    loop_->assertInLoopThread();
    assert(state_ == Connecting);
    setState(Connected);
    channel_->tie_(shared_from_this());
    channel_->EnableRead();

    connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed() { LOG_DEBUG << " "; }

void TcpConnection::handle_ep_in()
{
    LOG_DEBUG << " fd = " << channel_->fd_() << " state = " << stateToString();
    loop_->assertInLoopThread();
    int savedErrno = 0;
    ssize_t n      = 0;
    while (1)
    {
        ssize_t cur = inputBuffer_.readFd(channel_->fd_(), &savedErrno);
        if (cur > 0)
        {
            n += cur;
        }
        else if (cur == 0)
        {
            LOG_ERROR << " EOF,should close";
            handle_ep_hup();
            return;
        }
        else
        {
            if (savedErrno == EINTR)
                continue;
            else if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK || savedErrno == 0)
            {
                break;
            }
            else
            {
                LOG_DEBUG << " errno=" << savedErrno << ",fd=" << channel_->fd_();
                // LOG_SYSERR << "TcpConnection::handleRead," << "fd=" << channel_->fd_();
                handle_ep_hup();
                return;
            }
        }
    }
    if (n > 0)
    {
        messageCallback_(shared_from_this(), &inputBuffer_);
    }
}

void TcpConnection::handle_ep_out()
{
    loop_->assertInLoopThread();
    LOG_DEBUG << " fd = " << channel_->fd_() << " state = " << stateToString();

    if (outputBuffer_.readableBytes() == 0)
    {
        channel_->DisableWrite();
        return;
    }

    bool written = false;
    while (outputBuffer_.readableBytes())
    {
        if (channel_->isWriting())
        {
            written   = true;
            ssize_t n = sockOption::write(channel_->fd_(), outputBuffer_.readBegin(), outputBuffer_.readableBytes());
            int err   = errno;
            if (n > 0)
            {
                outputBuffer_.retrieve(n);
            }
            else if (err == EAGAIN || err == EWOULDBLOCK)
                return;
            else
            {
                LOG_SYSERR << "TcpConnection::handleWrite";
                handle_ep_hup();
                break;
            }
        }
        else
        {
            LOG_TRACE << "Connection fd = " << channel_->fd_() << " is down, no more writing";
            return;
        }
    }
    if (outputBuffer_.readableBytes() == 0)
    {
        if (written)
        {
            channel_->DisableWrite();
            if (writeCompleteCallback_)
            {
                loop_->queueInLoop([this, conn = shared_from_this()] { writeCompleteCallback_(conn); });
            }
        }

        if (rdhup_phrase)
        {
            LOG_DEBUG << " rdhup_phrase";
            handle_ep_hup();
        }
    }
}

// finish read and write when EPOLLRDHUP,return whether SHUT_WR
bool TcpConnection::finish_in_out()
{
    LOG_DEBUG << " ";
    loop_->assertInLoopThread();
    int savedErrno = 0;
    ssize_t n      = 0;
    while (1)
    {
        ssize_t cur = inputBuffer_.readFd(channel_->fd_(), &savedErrno);
        if (cur > 0)
        {
            n += cur;
        }
        else if (cur == 0)
        {
            break;
        }
        else
        {
            if (savedErrno == EINTR) continue;
            if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK)
            {
                // 0% happen
                LOG_SYSERR << " TcpConnection::finish_in_out,unexpected errno";
                handle_ep_hup();
                return false;
            }
            else
            {
                // completely break;
                LOG_ERROR << " TcpConnection::finish_in_out,errno=" << savedErrno;
                handle_ep_hup();
                return false;
            }
            break;
        }
    }
    if (n > 0)
    {
        messageCallback_(shared_from_this(), &inputBuffer_);
    }

    //
    channel_->EnableWrite();
    if (outputBuffer_.readableBytes() == 0)
    {
        channel_->DisableWrite();
        return true;
    }

    bool written = false;
    while (outputBuffer_.readableBytes())
    {
        if (channel_->isWriting())
        {
            written = true;
            n       = sockOption::write(channel_->fd_(), outputBuffer_.readBegin(), outputBuffer_.readableBytes());
            int err = sockOption::getSocketError(channel_->fd_());
            if (n > 0)
            {
                outputBuffer_.retrieve(n);
            }
            // don't continue,will spin;wait for left EPOLLOUT,EPOLLHUP at the end
            else if (err == EAGAIN || err == EWOULDBLOCK)
                return false;
            else
            {
                // completely break;
                LOG_SYSERR << "TcpConnection::finish_in_out";
                handle_ep_hup();
                rdhup_phrase = true;
                return false;
            }
        }
        else
        {
            LOG_SYSERR << " TcpConnection::finish_in_out,unexpected errno,Connection fd = " << channel_->fd_() << " is down, no more writing";
            return true;
        }
    }

    if (outputBuffer_.readableBytes() == 0 && written)
    {
        channel_->DisableWrite();
        if (writeCompleteCallback_)
        {
            writeCompleteCallback_(shared_from_this());
        }
    }

    return true;
}

// EPOLLRDHUP
void TcpConnection::handle_ep_rdhup()
{
    LOG_DEBUG << " fd = " << channel_->fd_() << " state = " << stateToString();
    if (finish_in_out()) socket_->shutdownWrite();
}

// EPOLLHUP force close
void TcpConnection::handle_ep_hup()
{
    loop_->assertInLoopThread();
    LOG_DEBUG << " fd = " << channel_->fd_() << " state = " << stateToString();

    if (state_ == Connected || state_ == Disconnecting)
    {
        channel_->DisableAll();
        socket_->close();
        setState(Disconnected);
    }
    else
    {
        LOG_DEBUG << " wrong state_=" << state_;
    }

    closeCallback_(shared_from_this());
}

void TcpConnection::handle_ep_err()
{
    int err = sockOption::getSocketError(channel_->fd_());
    LOG_ERROR << "TcpConnection::handleError [" << name_ << "] - SO_ERROR = " << err << " " << strerrorInfo(err);
    if (err != 0) handle_ep_hup();
}
