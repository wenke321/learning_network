
#include "TcpConnection.h"

#include <fcntl.h>
#include <linux/futex.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <climits>
#include <cstring>
#include <functional>
#include <memory>
#include <string>

#include "Loggers/Logger.h"
#include "Memorys/Buffer.h"
#include "Sockets/Socket.h"
#include "Sockets/SocketOps.h"
#include "Timers/Timer.h"
#include "Timers/Timestamp.h"
#include "WeakCallback.h"
#include "basics/Channel.h"
#include "helpers/builtins.h"
#include "helpers/kw_micros.h"

int TcpConnection::get_fd() { return channel_->fd_(); }

void defaultConnectionCallback(const TcpConnectionPtr& conn)
{
    LOG_TRACE << conn->localAddress()->ipPortStr() << " -> " << conn->peerAddress()->ipPortStr() << " is " << (conn->connected() ? "UP" : "DOWN");
    // do not call conn->forceClose(), because some users want to register message callback only.
}

void defaultMessageCallback(const TcpConnectionPtr&, Buffer* buf) { buf->retrieveAll(); }

TcpConnection::TcpConnection(EventLoop* loop, const std::string& nameArg, int sockfd, const InetAddr* localAddr, const InetAddr* peerAddr, bool _keep_alive) : name_(nameArg), reading_(true), socket_(new Socket(sockfd)), localAddr_(localAddr), peerAddr_(peerAddr), highWaterMark_(64 * 1024 * 1024), loop_(loop)
{
    {
        LOG_DEBUG << " TcpConnection::ctor[" << name_ << "] at " << this << " fd=" << sockfd;
    }
    channel_ = loop_->add_channel(sockfd);
    channel_->set_in_callback([this] { handle_ep_in(); });
    channel_->set_out_callback([this] { handle_ep_out(); });
    channel_->set_rdhup_callback([this] { handle_ep_rdhup(); });
    channel_->set_hup_callback([this] { handle_ep_hup(); });
    channel_->set_err_callback([this] { handle_ep_err(); });
    if (_keep_alive) socket_->setKeepAlive(true);

    _store_release(&state_, Connecting);
}

TcpConnection::TcpConnection(Channel*& ch, const std::string& name, const InetAddr* localAddr, const InetAddr* peerAddr, bool _keep_alive) : state_(Connecting), name_(name), reading_(true), localAddr_(localAddr), peerAddr_(peerAddr), highWaterMark_(64 * 1024 * 1024)
{
    {
        LOG_DEBUG << " TcpConnection::ctor[" << name_ << "] at " << this << " fd=" << ch->fd_();
    }
    channel_ = ch;
    loop_    = channel_->owner_loop();
    socket_  = std::make_unique<Socket>(channel_->fd_());

    channel_->set_in_callback([this] { handle_ep_in(); });
    channel_->set_out_callback([this] { handle_ep_out(); });
    channel_->set_rdhup_callback([this] { handle_ep_rdhup(); });
    channel_->set_hup_callback([this] { handle_ep_hup(); });
    channel_->set_err_callback([this] { handle_ep_err(); });
    if (_keep_alive) socket_->setKeepAlive(true);

    _store_release(&state_, Connecting);
}

TcpConnection::~TcpConnection()
{
    atomic_ulong old_state = _load_relaxed(&state_);
    {
        LOG_DEBUG << " TcpConnection::dtor[" << name_ << "] at " << this << " fd=" << channel_->fd_() << " state=" << stateToString(old_state);
    }
    if (!(old_state & Disconnected))
    {
        LOG_DEBUG << " wrong state=" << stateToString(old_state) << ",should sync";
    }
}

EventLoop* TcpConnection::getLoop() const { return loop_; }
const std::string& TcpConnection::name() const { return name_; }

const InetAddr* TcpConnection::localAddress() const { return localAddr_; }
const InetAddr* TcpConnection::peerAddress() const { return peerAddr_; }

bool TcpConnection::connected() const { return state_ & Connected; }        // fix
bool TcpConnection::disconnected() const { return state_ & Disconnected; }  // fix

bool TcpConnection::getTcpInfo(struct tcp_info* tcpi) const { return socket_->getTcpInfo(tcpi); }

std::string TcpConnection::getTcpInfoString() const
{
    char buf[1024];
    buf[0] = '\0';
    socket_->getTcpInfoString(buf, sizeof(buf));
    return buf;
}

void TcpConnection::set_unused()
{
    atomic_ulong old_state = _load_acquire(&state_);
    {
        LOG_DEBUG << " fd=" << channel_->fd_() << ",state=" << stateToString(old_state);
    }

    if (old_state & Connecting)
    {
        futex_wait(&state_, Connecting);
        old_state = _load_acquire(&state_);
    }

    for (;;)
    {
        if (kw_unlikely(old_state & Disconnected))
        {
            {
                LOG_DEBUG << " hup already";
            }
            old_state = _load_acquire(&state_);
            return;
        }
        if (kw_unlikely(!(old_state & used)))
        {
            {
                LOG_DEBUG << " wrong state should sync or you call this func twice";
            }
            return;
        }
        if (old_state & writing)
        {
            futex_wait(&state_, old_state);
            old_state = _load_relaxed(&state_);
            continue;
        }
        else
        {
            if (!_CAS_strong_relaxed(&state_, &old_state, old_state - used)) continue;
            futex_wake(&state_);
            break;
        }
    }
}

void TcpConnection::send(const void* data, int len) { send(stringPiece(static_cast<const char*>(data), len)); }

void TcpConnection::send(const stringPiece& message)
{
    atomic_ulong old_state = _load_acquire(&state_);
    {
        LOG_DEBUG << " fd=" << channel_->fd_() << ",state=" << stateToString(old_state);
    }

    if (old_state & Connecting)
    {
        futex_wait(&state_, Connecting);
        old_state = _load_acquire(&state_);
    }

    for (;;)
    {
        if (kw_unlikely(old_state & Disconnected))
        {
            {
                LOG_DEBUG << " hup already";
            }
            old_state = _load_acquire(&state_);
            return;
        }
        if (kw_unlikely(!(old_state & used)))
        {
            {
                LOG_DEBUG << " send must before set_unused !!!" << " state=" << stateToString(old_state);
            }
            return;
        }
        if (old_state & writing)
        {
            if (!_CAS_strong_relaxed(&state_, &old_state, old_state + (1 << 6))) continue;
            break;
        }
        else
        {
            if (!_CAS_strong_relaxed(&state_, &old_state, old_state | writing))
            {
                old_state = _load_acquire(&state_);
                continue;
            }
            old_state |= writing;
            while (!_CAS_strong_relaxed(&state_, &old_state, old_state + (1 << senders_offset)))
            {
                if (old_state & Disconnected)
                {
                    {
                        LOG_DEBUG << " hup already";
                    }
                    old_state = _load_acquire(&state_);
                    return;
                }
                else
                {
                    {
                        LOG_DEBUG << " impossible state,should sync" << ",state=" << stateToString(old_state);
                    }
                    return;
                }
            }
            break;
        }
    }

    if (loop_->isInLoopThread())
    {
        sendInLoop(message);
    }
    else
    {
        // void (TcpConnection::*fp)(const stringPiece& message) = &TcpConnection::sendInLoop;
        loop_->queueInLoop([=] { sendInLoop(message); });
        // std::forward<string>(message)));
    }
}

// FIXME efficiency!!!
void TcpConnection::send(Buffer* buf)
{
    atomic_ulong old_state = _load_acquire(&state_);
    {
        LOG_DEBUG << " fd=" << channel_->fd_() << ",state=" << stateToString(old_state);
    }

    if (old_state & Connecting)
    {
        futex_wait(&state_, Connecting);
        old_state = _load_acquire(&state_);
    }

    for (;;)
    {
        if (kw_unlikely(old_state & Disconnected))
        {
            {
                LOG_DEBUG << " hup already";
            }
            old_state = _load_acquire(&state_);
            return;
        }
        if (kw_unlikely(!(old_state & used)))
        {
            {
                LOG_DEBUG << " send must before set_unused !!!" << " state=" << stateToString(old_state);
            }
            return;
        }
        if (old_state & writing)
        {
            if (!_CAS_strong_relaxed(&state_, &old_state, old_state + (1 << 6))) continue;
            break;
        }
        else
        {
            if (!_CAS_strong_relaxed(&state_, &old_state, old_state | writing))
            {
                old_state = _load_acquire(&state_);
                continue;
            }
            old_state |= writing;
            while (!_CAS_strong_relaxed(&state_, &old_state, old_state + (1 << senders_offset)))
            {
                if (old_state & Disconnected)
                {
                    {
                        LOG_DEBUG << " hup already";
                    }
                    old_state = _load_acquire(&state_);
                    return;
                }
                else
                {
                    {
                        LOG_DEBUG << " impossible state,should sync" << ",state=" << stateToString(old_state);
                    }
                    return;
                }
            }
            break;
        }
    }

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

void TcpConnection::send_OOB(const stringPiece& _msg)
{
    atomic_ulong old_state = _load_acquire(&state_);
    {
        LOG_DEBUG << " fd=" << channel_->fd_() << ",state=" << stateToString(old_state);
    }

    if (old_state & Connecting)
    {
        futex_wait(&state_, Connecting);
        old_state = _load_acquire(&state_);
    }

    for (;;)
    {
        if (kw_unlikely(old_state & Disconnected))
        {
            {
                LOG_DEBUG << " hup already";
            }
            old_state = _load_acquire(&state_);
            return;
        }
        if (kw_unlikely(!(old_state & used)))
        {
            {
                LOG_DEBUG << " send must before set_unused !!!" << " state=" << stateToString(old_state);
            }
            return;
        }
        if (old_state & writing)
        {
            if (!_CAS_strong_relaxed(&state_, &old_state, old_state + (1 << 6))) continue;
            break;
        }
        else
        {
            if (!_CAS_strong_relaxed(&state_, &old_state, old_state | writing))
            {
                old_state = _load_acquire(&state_);
                continue;
            }
            old_state |= writing;
            while (!_CAS_strong_relaxed(&state_, &old_state, old_state + (1 << senders_offset)))
            {
                if (old_state & Disconnected)
                {
                    {
                        LOG_DEBUG << " hup already";
                    }
                    old_state = _load_acquire(&state_);
                    return;
                }
                else
                {
                    {
                        LOG_DEBUG << " impossible state,should sync" << ",state=" << stateToString(old_state);
                    }
                    return;
                }
            }
            break;
        }
    }

    if (loop_->isInLoopThread())
    {
        sendInLoop(_msg);
    }
    else
    {
        loop_->queueInLoop([=] { sendInLoop(_msg); });
    }
}

void TcpConnection::sendInLoop(const stringPiece message) { sendInLoop(message.data(), message.size()); }

void TcpConnection::sendInLoop(const void* data, size_t len)
{
    loop_->assertInLoopThread();

    atomic_ulong old_state = _load_relaxed(&state_);
    {
        LOG_DEBUG << " fd=" << channel_->fd_() << ",state=" << stateToString(old_state);
    }

    ssize_t nwrote   = 0;
    size_t remaining = len;
    bool faultError  = false;

    // if no thing in output queue, try writing directly
    if (!channel_->isWriting() && outputBuffers_.empty())
    {
        while (remaining)
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
                int err = sockOption::getSocketError(channel_->fd_());
                if (err == EAGAIN)
                {
                    break;
                }
                else if (err == EINTR)
                    continue;
                else
                {
                    {
                        LOG_SYSERR << "TcpConnection::sendInLoop," << " SO_ERROR = " << err << " " << strerrorInfo(err);
                    }
                    faultError = true;
                    break;
                }
            }
        }
    }

    assert(remaining <= len);
    if (!faultError && remaining > 0)  // EAGAIN
    {
        Buffer buf(remaining);
        oldLen += remaining;
        if (oldLen >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen));
        }
        buf.append(static_cast<const char*>(data) + nwrote, remaining);
        outputBuffers_.emplace_front(std::move(buf));
        if (!channel_->isWriting())
        {
            channel_->EnableWrite();
        }
    }

    for (;;)
    {
        if (kw_unlikely(old_state & Disconnected))
        {
            {
                LOG_DEBUG << " hup already";
            }
            old_state = _load_acquire(&state_);
            return;
        }
        if (kw_unlikely(!(old_state & used)))
        {
            {
                LOG_DEBUG << " send must before set_unused !!!,state=" << stateToString(old_state);
            }
            return;
        }
        if (old_state & writing)
        {
            while (!_CAS_strong_relaxed(&state_, &old_state, old_state - (1 << senders_offset)))
            {
                if (kw_unlikely(old_state & Disconnected))
                {
                    {
                        LOG_DEBUG << " hup already";
                    }
                    old_state = _load_acquire(&state_);
                    return;
                }

                // debug
                {
                    LOG_DEBUG << " impossible state,should sync,state=" << stateToString(old_state);
                }
                return;
            }
            if (!((old_state >> senders_offset) ^ 1))
            {
                old_state ^= (1 << senders_offset);
                {
                    LOG_DEBUG << " maybe last sender";
                }
                if (!_CAS_strong_relaxed(&state_, &old_state, old_state ^ writing))
                {
                    if (kw_unlikely(old_state & Disconnected))
                    {
                        {
                            LOG_DEBUG << " hup already";
                        }
                        old_state = _load_acquire(&state_);
                        return;
                    }

                    // debug
                    {
                        LOG_DEBUG << " impossible state,should sync,state=" << stateToString(old_state);
                    }
                    return;
                }
                else
                    futex_wake(&state_);
                // debug
                {
                    LOG_DEBUG << " send over,state=" << stateToString(_load_relaxed(&state_));
                }
            }
            break;
        }
        // debug
        else
        {
            {
                LOG_DEBUG << " impossible state,should sync,state=" << stateToString(old_state);
            }
            return;
        }
    }
}

void TcpConnection::shutdown()
{
    atomic_ulong old_state = _load_acquire(&state_);
    {
        LOG_DEBUG << " fd=" << channel_->fd_() << " local addr=" << localAddr_->ipPortStr() << ",state=" << stateToString(old_state);
    }

    if (old_state & Connecting)
    {
        futex_wait(&state_, Connecting);
        old_state = _load_acquire(&state_);
    }

    for (;;)
    {
        if (old_state & Disconnected)
        {
            {
                LOG_DEBUG << " hup before here";
            }
            old_state = _load_acquire(&state_);
            return;
        }
        // multi shutdown
        else if (old_state & Disconnecting)
        {
            {
                LOG_DEBUG << " other thread is shutdown";
            }
            return;
        }
        // connected
        else
        {
            if (old_state & used)
            {
                futex_wait(&old_state, (Connected | used));
                old_state = _load_relaxed(&state_);
                continue;
            }
            else
            {
                if (!_CAS_strong_relaxed(&state_, &old_state, Disconnecting)) continue;
                break;
            }
        }
    }

    // debug
    old_state = _load_relaxed(&state_);
    {
        LOG_DEBUG << " fd=" << channel_->fd_() << ",state=" << stateToString(old_state);
    }

    loop_->runInLoop([conn_ = shared_from_this()] { conn_->shutdownInLoop(); });
}

void TcpConnection::shutdownInLoop()
{
    {
        LOG_DEBUG << " fd=" << channel_->fd_();
    }
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
    {
        LOG_DEBUG << " fd=" << channel_->fd_();
    }
    loop_->runInLoop([&] { forceCloseInLoop(); });
}

void TcpConnection::forceCloseWithDelay(double seconds)
{
    LOG_DEBUG << " fd=" << channel_->fd_();
    if (state_ == Connected || state_ == Disconnecting)
    {
        setState(Disconnecting);
        // loop_->runAfter(seconds, makeWeakCallback(shared_from_this(),
        //                                           &TcpConnection::forceClose));
        Timer* timer = new Timer(Timestamp::now_microsecconds() + seconds * 1000000, [conn = shared_from_this()] { conn->forceCloseInLoop(); }, 0);
        loop_->addTimer(timer);
    }
    else
    {
        LOG_DEBUG << " wrong state_=" << state_ << ",should sync";
    }
}

void TcpConnection::forceCloseInLoop()
{
    {
        LOG_DEBUG << " fd=" << channel_->fd_();
    }
    loop_->assertInLoopThread();
    set_unused();
    handle_ep_hup();
}

std::string TcpConnection::stateToString(int _state) const
{
    std::string ret = "";
    if (_state & Connecting) ret += "Connecting|";
    if (_state & Connected) ret += "Connected|";
    if (_state & Disconnecting) ret += "Disconnecting|";
    if (_state & Disconnected) ret += "Disconnected|";
    if (_state & used) ret += "used|";
    if (_state & writing) ret += "writing|";
    if (_state >> senders_offset) ret += "have sender";
    return ret;
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

void TcpConnection::setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }

void TcpConnection::setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }

void TcpConnection::set_OOB_callback(std::function<void(const TcpConnectionPtr&, char)> _cb) { OOB_messageCallback_ = _cb; }

void TcpConnection::setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }

void TcpConnection::setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }

void TcpConnection::setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
{
    highWaterMarkCallback_ = cb;
    highWaterMark_         = highWaterMark;
}

void TcpConnection::connectEstablished()
{
    {
        LOG_DEBUG << " fd=" << channel_->fd_();
    }
    loop_->assertInLoopThread();

    atomic_ulong old_state = _load_acquire(&state_);
    if (old_state != Connecting)
    {
        LOG_DEBUG << " wrong state_=" << state_ << ",should sync";
    }

    channel_->tie_(shared_from_this());
    channel_->EnableRead();

    _store_release(&state_, (Connected | used));
    futex_wake(&state_);

    connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed()
{
    if (_load_relaxed(&state_) != Disconnected)
    {
        LOG_DEBUG << " wrong state_=" << state_ << ",should sync";
    }
}

void TcpConnection::handle_ep_in()
{
    loop_->assertInLoopThread();

    // debug
    atomic_ulong old_state = _load_relaxed(&state_);
    {
        LOG_DEBUG << " fd = " << channel_->fd_() << " state = " << stateToString(old_state);
    }

    int savedErrno = 0;
    ssize_t n      = 0;
    while (1)
    {
        ssize_t cur = inputBuffer_.readFd(channel_->fd_(), &savedErrno);
        if (cur > 0)
        {
            n += cur;
            continue;
        }
        else if (cur == 0)
        {
            {
                LOG_ERROR << " EOF,should close";
            }
            if (n > 0)
            {
                messageCallback_(shared_from_this(), &inputBuffer_);
            }
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
                {
                    LOG_DEBUG << " errno=" << savedErrno << ",fd=" << channel_->fd_();
                }
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

void TcpConnection::handle_ep_pri()
{
    loop_->assertInLoopThread();

    // debug
    atomic_ulong old_state = _load_relaxed(&state_);
    {
        LOG_DEBUG << " fd = " << channel_->fd_() << " state = " << stateToString(old_state);
    }

    char oob;
    int savedErrno = 0;
    ssize_t n      = 0;
    while (1)
    {
        ssize_t cur = ::recv(channel_->fd_(), &oob, 1, MSG_OOB);
        savedErrno  = errno;
        if (cur > 0)
        {
            n += cur;
            continue;
        }
        else if (cur == 0)
        {
            {
                LOG_ERROR << " EOF,should close";
            }
            if (n > 0)
            {
                messageCallback_(shared_from_this(), &inputBuffer_);
            }
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
                {
                    LOG_DEBUG << " errno=" << savedErrno << ",fd=" << channel_->fd_();
                }
                handle_ep_hup();
                return;
            }
        }
    }
    if (n == 1)
    {
        OOB_messageCallback_(shared_from_this(), oob);
    }
    else
    {
        LOG_SYSERR << " oob != 1 byte";
    }
}

void TcpConnection::handle_ep_out()
{
    loop_->assertInLoopThread();

    // debug
    {
        LOG_DEBUG << " fd = " << channel_->fd_() << " state = " << stateToString(_load_relaxed(&state_));
    }

    if (outputBuffers_.empty())
    {
        channel_->DisableWrite();
        return;
    }

    int left, fd = channel_->fd_();
    while (!outputBuffers_.empty())
    {
        Buffer& buf = outputBuffers_.front();
        left        = buf.readableBytes();
        while (left)
        {
            ssize_t n = sockOption::write(fd, buf.readBegin(), left);
            if (n > 0)
            {
                buf.retrieve(n);
            }
            else
            {
                int err = sockOption::getSocketError(fd);
                if (err == EAGAIN)
                    return;
                else if (err == EINTR)
                    continue;
                else
                {
                    {
                        LOG_SYSERR << "TcpConnection::handleWrite," << " SO_ERROR = " << err << " " << strerrorInfo(err);
                    }
                    handle_ep_hup();
                    outputBuffers_.clear();
                    break;
                }
            }
        }
        if (buf.readableBytes() == 0)
        {
            if (writeCompleteCallback_)
            {
                loop_->queueInLoop([this, conn = shared_from_this()] { writeCompleteCallback_(conn); });
            }
            outputBuffers_.pop_front();
        }
    }
    if (rdhup_phrase)
    {
        {
            LOG_DEBUG << " rdhup_phrase";
        }
        handle_ep_hup();
    }
}

// finish read and write when EPOLLRDHUP
// return whether SHUT_WR
bool TcpConnection::finish_in_out()
{
    loop_->assertInLoopThread();

    {
        LOG_DEBUG << " fd = " << channel_->fd_();
    }

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
                {
                    LOG_SYSERR << " TcpConnection::finish_in_out,unexpected errno";
                }
                handle_ep_hup();
                return false;
            }
            else
            {
                // completely break;
                {
                    LOG_ERROR << " TcpConnection::finish_in_out,errno=" << savedErrno;
                }
                handle_ep_hup();
                return false;
            }
        }
    }
    if (n > 0)
    {
        messageCallback_(shared_from_this(), &inputBuffer_);
    }

    //
    int left, fd = channel_->fd_();
    while (!outputBuffers_.empty())
    {
        Buffer& buf = outputBuffers_.front();
        left        = buf.readableBytes();
        while (left)
        {
            ssize_t n = sockOption::write(fd, buf.readBegin(), left);
            if (n > 0)
            {
                buf.retrieve(n);
            }
            else
            {
                int err = sockOption::getSocketError(fd);
                if (err == EAGAIN)
                    return false;
                else if (err == EINTR)
                    continue;
                else
                {
                    {
                        LOG_SYSERR << "TcpConnection::handleWrite," << " SO_ERROR = " << err << " " << strerrorInfo(err);
                    }
                    handle_ep_hup();
                    // outputBuffers_.clear();
                    return false;
                }
            }
        }
        if (buf.readableBytes() == 0)
        {
            if (writeCompleteCallback_)
            {
                loop_->queueInLoop([this, conn = shared_from_this()] { writeCompleteCallback_(conn); });
            }
            outputBuffers_.pop_front();
        }
    }

    return true;
}

// EPOLLRDHUP
void TcpConnection::handle_ep_rdhup()
{
    atomic_ulong old_state = _load_acquire(&state_);
    {
        LOG_DEBUG << " fd = " << channel_->fd_() << " state = " << stateToString(old_state);
    }

    if (old_state & Connecting)
    {
        futex_wait(&state_, old_state);
        old_state = _load_acquire(&state_);
    }

    if (finish_in_out()) handle_ep_hup();
    rdhup_phrase = true;
}

// EPOLLHUP force close
void TcpConnection::handle_ep_hup()
{
    loop_->assertInLoopThread();
    atomic_ulong old_state = _load_acquire(&state_);
    {
        LOG_DEBUG << " fd = " << channel_->fd_() << " state = " << stateToString(old_state);
    }

    if (old_state & Disconnected)
    {
        {
            LOG_DEBUG << " already Disconnected";
        }
        return;
    }

    channel_->DisableAll();
    channel_->reset_callbacks();
    channel_->reset_listen_events();
    socket_->close();

    _store_release(&state_, Disconnected);
    futex_wake(&state_);

    closeCallback_(shared_from_this());
}

void TcpConnection::handle_ep_err()
{
    int err = sockOption::getSocketError(channel_->fd_());
    {
        LOG_ERROR << "TcpConnection::handleError [" << name_ << "] - SO_ERROR = " << err << " " << strerrorInfo(err);
    }
    if (err != 0) handle_ep_hup();
}
