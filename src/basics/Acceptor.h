
#pragma once
#include <functional>
#include <memory>

#include "../Sockets/Socket.h"
#include "Channel.h"

class Socket;
class Channel;

class Acceptor
{
   public:
    typedef std::function<void(int sockfd, const InetAddr*)> NewConnectionCallback;

    Acceptor(EventLoop* loop, const InetAddr& listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb) { newConnectionCallback_ = cb; }

    void listen();
    void stop();

    bool listening() const { return listening_; }

    // Deprecated, use the correct spelling one above.
    // Leave the wrong spelling here in case one needs to grep it for error messages.
    // bool listenning() const { return listening(); }

   private:
    void handleRead();

    EventLoop* loop_;
    Socket acceptSocket_;
    Channel* acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listening_;
    int idleFd_;
};
