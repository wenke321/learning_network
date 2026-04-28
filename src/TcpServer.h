
#pragma once

#include <cstdint>
#include <map>

#include "EventloopThreadPool.h"
#include "SignalHandler.h"
#include "TcpConnection.h"

class TcpServer
{
   public:
    typedef std::function<void(EventLoop*)> ThreadInitCallback;
    enum Option
    {
        NoReusePort,
        ReusePort,
    };

    // TcpServer(EventLoop* loop, const InetAddress& listenAddr);
    TcpServer(EventLoop* loop, const InetAddr& listenAddr, const std::string& nameArg, Option option = NoReusePort);
    ~TcpServer();  // force out-line dtor, for std::unique_ptr members.

    const std::string& ipPort() const { return ipPort_; }
    const std::string& name() const { return name_; }
    EventLoop* getLoop() const { return loop_; }

    /// Set the number of threads for handling input.
    ///
    /// Always accepts new connection in loop's thread.
    /// Must be called before @c start
    /// @param numThreads
    /// - 0 means all I/O in loop's thread, no thread will created.
    ///   this is the default value.
    /// - 1 means all I/O in another thread.
    /// - N means a thread pool with N threads, new connections
    ///   are assigned on a round-robin basis.
    void setThreadNum(int numThreads);
    void setThreadInitCallback(const ThreadInitCallback& cb) { threadInitCallback_ = cb; }
    /// valid after calling start()
    std::shared_ptr<EventloopThreadPool> threadPool() { return threadPool_; }

    /// Starts the server if it's not listening.
    ///
    /// It's harmless to call it multiple times.
    /// Thread safe.
    void start();

    /// Set connection callback.
    /// Not thread safe.
    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }

    /// Set message callback.
    /// Not thread safe.
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }

    /// Set write complete callback.
    /// Not thread safe.
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }

   private:
    /// Not thread safe, but in loop
    void newConnection(int sockfd, const InetAddr* peerAddr);

    void handle_signal();
    /// Thread safe.
    void removeConnection(TcpConnectionPtr conn);
    /// Not thread safe, but in loop
    void removeConnectionInLoop(TcpConnectionPtr conn);

    typedef std::map<std::string, TcpConnectionPtr> ConnectionMap;

    EventLoop* loop_;  // the acceptor loop
    const std::string ipPort_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_;  // avoid revealing Acceptor
    std::shared_ptr<EventloopThreadPool> threadPool_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    ThreadInitCallback threadInitCallback_;
    volatile uint32_t started_;
    // always in loop thread
    int nextConnId_;
    ConnectionMap connections_;
    SignalHandler signal_handler;
};
