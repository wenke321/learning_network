#pragma once

#include <openssl/crypto.h>
#include <openssl/ssl.h>

#include <memory>

#include "../TcpConnection.h"

class ssl_TcpConnection : public TcpConnection
{
   public:
    ssl_TcpConnection(EventLoop* loop, const std::string& name, int sockfd, const InetAddr* localAddr, const InetAddr* peerAddr, bool _keep_alive, SSL* ssl);
    ssl_TcpConnection(Channel*&, const std::string& name, const InetAddr* localAddr, const InetAddr* peerAddr, bool _keep_alive, SSL* ssl);
    ~ssl_TcpConnection();

   private:
    // 必须重写读、写、关闭行为
    void handle_ep_in() override;
    void handle_ep_out() override;
    bool finish_in_out() override;
    void sendInLoop(const void* data, size_t len) override;
    void handle_ep_hup() override;
};

typedef std::shared_ptr<ssl_TcpConnection> ssl_TcpConnectionPtr;