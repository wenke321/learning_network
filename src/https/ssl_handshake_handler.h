#pragma once
#include <openssl/ssl.h>

#include <functional>
#include <memory>

#include "../EventLoop.h"
#include "../basics/Channel.h"

class ssl_handshake_handler : public std::enable_shared_from_this<ssl_handshake_handler>
{
   public:
    using handshake_callback = std::function<void(SSL*, int sockfd)>;
    using error_callback     = std::function<void(int sockfd)>;

    ssl_handshake_handler(EventLoop* loop, Channel* _ch, SSL* ssl, handshake_callback successCb, error_callback errorCb);

    void start();

   private:
    void doHandshake();
    void onError();

    EventLoop* loop_;
    int sockfd_;
    SSL* ssl_;
    handshake_callback successCb_;
    error_callback errorCb_;
    Channel* channel_;
    // 超时定时器...
};