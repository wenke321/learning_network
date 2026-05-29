#include "ssl_handshake_handler.h"

#include "../Loggers/Logger.h"

ssl_handshake_handler::ssl_handshake_handler(EventLoop* loop, Channel* _ch, SSL* ssl, handshake_callback successCb, error_callback errorCb) : loop_(loop), channel_(_ch), sockfd_(_ch->fd_()), ssl_(ssl), successCb_(std::move(successCb)), errorCb_(std::move(errorCb))
{
    channel_->set_in_callback([this] { doHandshake(); });
    channel_->set_out_callback([this] { doHandshake(); });
    channel_->set_hup_callback([this] { onError(); });
    channel_->set_err_callback([this] { onError(); });
    // 设置超时定时器（可选）
}

void ssl_handshake_handler::start()
{
    {
        LOG_DEBUG << " ";
    }
    channel_->tie_(shared_from_this());
    channel_->EnableRead();
    channel_->EnableWrite();  // SSL_connect 可能需要写
    doHandshake();
}

void ssl_handshake_handler::doHandshake()
{
    {
        LOG_DEBUG << " ";
    }
    int ret = SSL_connect(ssl_);
    if (ret == 1)
    {
        // 握手成功
        channel_->DisableAll();
        successCb_(ssl_, sockfd_);
        // 注意：这里不再持有 ssl_ 和 sockfd_，所有权交给回调
    }
    else
    {
        int err = SSL_get_error(ssl_, ret);
        if (err == SSL_ERROR_WANT_READ)
        {
            channel_->EnableRead();
            channel_->DisableWrite();
        }
        else if (err == SSL_ERROR_WANT_WRITE)
        {
            channel_->EnableWrite();
            channel_->DisableRead();
        }
        else
        {
            // 握手失败
            onError();
        }
    }
}

void ssl_handshake_handler::onError()
{
    {
        LOG_DEBUG << " ";
    }
    SSL_free(ssl_);
    ssl_ = nullptr;
    errorCb_(sockfd_);
}