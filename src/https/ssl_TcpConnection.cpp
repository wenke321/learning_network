#include "ssl_TcpConnection.h"

#include <openssl/err.h>

#include "../Loggers/Logger.h"
#include "../helpers/kw_micros.h"

ssl_TcpConnection::ssl_TcpConnection(EventLoop* loop, const std::string& name, int sockfd, const InetAddr* localAddr, const InetAddr* peerAddr, bool _keep_alive, SSL* ssl) : TcpConnection(loop, name, sockfd, localAddr, peerAddr, _keep_alive) { ssl_ = ssl; }

ssl_TcpConnection::ssl_TcpConnection(Channel*& _ch, const std::string& name, const InetAddr* localAddr, const InetAddr* peerAddr, bool _keep_alive, SSL* ssl) : TcpConnection(_ch, name, localAddr, peerAddr, _keep_alive) { ssl_ = ssl; }

ssl_TcpConnection::~ssl_TcpConnection() {}

void ssl_TcpConnection::handle_ep_in()
{
    loop_->assertInLoopThread();

    // debug
    atomic_ulong old_state = _load_acquire(&state_);
    {
        LOG_DEBUG << " fd = " << channel_->fd_() << " state = " << stateToString(old_state);
    }

    ssize_t n = 0;
    while (true)
    {
        inputBuffer_.ensureWritableBytes(65536);
        ssize_t cur = SSL_read(ssl_, inputBuffer_.writeBegin(), inputBuffer_.writableBytes());
        if (cur > 0)
        {
            inputBuffer_.hasWritten(cur);
            n += cur;
            continue;
        }

        int sslErr = SSL_get_error(ssl_, cur);
        if (sslErr == SSL_ERROR_WANT_READ)
        {
            break;
        }
        else if (sslErr == SSL_ERROR_WANT_WRITE)
        {
            channel_->EnableWrite();
            break;
        }
        else if (sslErr == SSL_ERROR_ZERO_RETURN)
        {
            if (n > 0) messageCallback_(shared_from_this(), &inputBuffer_);
            handle_ep_hup();
            return;
        }
        else
        {
            LOG_ERROR << "SSL_read error: " << sslErr;
            handle_ep_hup();
            return;
        }
    }
    if (n > 0)
    {
        messageCallback_(shared_from_this(), &inputBuffer_);
    }
}

void ssl_TcpConnection::handle_ep_out()
{
    loop_->assertInLoopThread();

    // debug
    atomic_ulong old_state = _load_acquire(&state_);
    {
        LOG_DEBUG << " fd = " << channel_->fd_() << " state = " << stateToString(old_state);
    }

    if (outputBuffers_.empty())
    {
        channel_->DisableWrite();
        return;
    }

    while (!outputBuffers_.empty())
    {
        Buffer& buf = outputBuffers_.front();
        while (buf.readableBytes())
        {
            ssize_t n = SSL_write(ssl_, buf.readBegin(), buf.readableBytes());
            if (n > 0)
            {
                buf.retrieve(n);
            }
            else
            {
                int sslErr = SSL_get_error(ssl_, n);
                if (sslErr == SSL_ERROR_WANT_WRITE)
                {
                    return;
                }
                else if (sslErr == SSL_ERROR_WANT_READ)
                {
                    channel_->EnableRead();
                    return;
                }
                else
                {
                    {
                        LOG_ERROR << "SSL_write error: " << sslErr;
                    }
                    handle_ep_hup();
                    return;
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
        handle_ep_hup();
    }
}

void ssl_TcpConnection::sendInLoop(const void* data, size_t len)
{
    loop_->assertInLoopThread();

    // debug
    atomic_ulong old_state = _load_relaxed(&state_);
    {
        LOG_DEBUG << " fd=" << channel_->fd_() << ",state=" << stateToString(old_state);
    }

    // 和普通 TcpConnection 类似，但用 SSL_write 替代 sockOption::write
    ssize_t nwrote   = 0;
    size_t remaining = len;
    bool faultError  = false;

    if (!channel_->isWriting() && outputBuffers_.empty())
    {
        while (remaining)
        {
            nwrote = SSL_write(ssl_, data, len);
            if (nwrote >= 0)
            {
                remaining = len - nwrote;
                if (remaining == 0 && writeCompleteCallback_)
                {
                    loop_->queueInLoop([this] { writeCompleteCallback_(shared_from_this()); });
                }
            }
            else
            {
                int err = SSL_get_error(ssl_, nwrote);
                if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ)
                {
                    if (err == SSL_ERROR_WANT_READ) channel_->EnableRead();
                    break;
                }
                faultError = true;
                break;
            }
        }
    }

    if (!faultError && remaining > 0)
    {
        Buffer buf;
        oldLen += remaining;
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        buf.append(static_cast<const char*>(data) + nwrote, remaining);
        outputBuffers_.emplace_front(std::move(buf));
        if (!channel_->isWriting()) channel_->EnableWrite();
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

void ssl_TcpConnection::handle_ep_hup()
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

    // SSL 干净关闭
    if (ssl_)
    {
        SSL_shutdown(ssl_);  // 可忽略返回值
        SSL_free(ssl_);
        ssl_ = nullptr;
    }

    set_unused();
    channel_->DisableAll();
    socket_->close();

    closeCallback_(shared_from_this());

    _store_release(&state_, Disconnected);
    futex_wake(&state_);
}

bool ssl_TcpConnection::finish_in_out()
{
    {
        LOG_DEBUG << " fd = " << channel_->fd_();
    }
    loop_->assertInLoopThread();
    int sslErr = 0;
    ssize_t n  = 0;
    // ---------- 加密读循环 ----------
    while (true)
    {
        inputBuffer_.ensureWritableBytes(65536);
        ssize_t cur = SSL_read(ssl_, inputBuffer_.writeBegin(), inputBuffer_.writableBytes());
        if (cur > 0)
        {
            inputBuffer_.hasWritten(cur);
            n += cur;
            continue;
        }

        sslErr = SSL_get_error(ssl_, cur);
        if (sslErr == SSL_ERROR_ZERO_RETURN)
        {
            // 对端干净关闭，相当于 TCP 的 EOF
            break;
        }
        else if (sslErr == SSL_ERROR_WANT_READ)
        {
            // 原逻辑在 finish_in_out 中遇到 EAGAIN 会直接关连接，
            // SSL 下保持相同处理：非预期等待视为错误
            {
                LOG_ERROR << "SSL_read WANT_READ in finish_in_out, closing";
            }
            handle_ep_hup();
            return false;
        }
        else if (sslErr == SSL_ERROR_WANT_WRITE)
        {
            // 读方向需要写，这里不应该发生，但仍然按错误处理
            {
                LOG_ERROR << "SSL_read WANT_WRITE in finish_in_out, closing";
            }
            handle_ep_hup();
            return false;
        }
        else
        {
            char buf[256];
            ERR_error_string_n(sslErr, buf, sizeof(buf));
            {
                LOG_ERROR << "SSL_read,err_code=" << sslErr << ",msg=" << buf;
            }
            handle_ep_hup();
            return false;
        }
    }

    if (n > 0)
    {
        messageCallback_(shared_from_this(), &inputBuffer_);
    }

    // ---------- 加密写循环 ----------
    if (outputBuffers_.empty())
    {
        return true;
    }

    while (!outputBuffers_.empty())
    {
        Buffer& buf = outputBuffers_.front();
        while (buf.readableBytes())
        {
            ssize_t written = SSL_write(ssl_, buf.readBegin(), buf.readableBytes());
            if (written > 0)
            {
                buf.retrieve(written);
            }
            else
            {
                sslErr = SSL_get_error(ssl_, written);
                if (sslErr == SSL_ERROR_WANT_WRITE)
                {
                    // 需要等待 socket 可写
                    if (!channel_->isWriting()) channel_->EnableWrite();
                    return false;
                }
                else if (sslErr == SSL_ERROR_WANT_READ)
                {
                    // 写方向需要读数据，启用读事件，等待下次事件
                    channel_->EnableRead();
                    return false;
                }
                else
                {
                    {
                        LOG_ERROR << "SSL_write error " << sslErr << " in finish_in_out";
                    }
                    handle_ep_hup();
                    return false;
                }
            }
        }

        if (writeCompleteCallback_)
        {
            writeCompleteCallback_(shared_from_this());
            outputBuffers_.pop_front();
        }
    }
    return true;
}