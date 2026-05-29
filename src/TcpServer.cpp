
#include "TcpServer.h"

#include <cassert>
#include <csignal>
#include <memory>

#include "EventLoop.h"
#include "Loggers/Logger.h"
#include "SignalHandler.h"
#include "Sockets/InetAddr.h"
#include "Sockets/SocketOps.h"
#include "TcpConnection.h"
#include "basics/Acceptor.h"
#include "basics/Channel.h"
#include "helpers/builtins.h"

TcpServer::TcpServer(EventLoop* loop, const InetAddr& listenAddr, const std::string& nameArg, Option option) : loop_(loop), ipPort_(listenAddr.ipPortStr()), name_(nameArg), threadPool_(new EventloopThreadPool(loop, name_)), connectionCallback_(defaultConnectionCallback), messageCallback_(defaultMessageCallback), nextConnId_(1), acceptor_(new Acceptor(loop, listenAddr, option == ReusePort)), signal_handler()
{
    acceptor_->setNewConnectionCallback([=](int sockfd, const InetAddr* peerAddr_) { newConnection(sockfd, peerAddr_); });

    signal_handler.set_handle_int([&] { handle_sig_int(); });
    signal_handler.init(loop_);
    signal(SIGPIPE, SIG_IGN);
}

TcpServer::~TcpServer()
{
    loop_->assertInLoopThread();
    LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";

    if (!connections_.empty())
    {
        LOG_FATAL << " why you didn't close all connections before here ?";
    }
}

void TcpServer::setThreadNum(int numThreads)
{
    assert(0 <= numThreads);
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{
    if (started_ == 0)
    {
        threadPool_->start(threadInitCallback_);

        assert(!acceptor_->listening());
        loop_->runInLoop([this] { acceptor_->listen(); });
    }
}

void TcpServer::newConnection(int sockfd, const InetAddr* peerAddr)
{
    LOG_TRACE << "TcpServer::newConnection";
    loop_->assertInLoopThread();
    EventLoop* ioLoop = threadPool_->getNextLoop();
    char buf[64];
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    {
        LOG_INFO << "TcpServer::newConnection [" << name_ << "] - new connection [" << connName << "] from " << peerAddr->ipPortStr();
    }
    // FIXME poll with zero timeout to double confirm the new connection
    // FIXME use make_shared if necessary
    InetAddr* localAddr   = new InetAddr(sockOption::getLocalAddr(sockfd));
    bool keep_alive       = sockOption::get_keep_alive(sockfd);
    TcpConnectionPtr conn = std::make_shared<TcpConnection>(ioLoop, connName, sockfd, localAddr, peerAddr, keep_alive);
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback([this](TcpConnectionPtr _conn) { removeConnection(_conn); });

    connections_[connName] = conn;

    // sync
    ioLoop->runInLoop([=] { conn->connectEstablished(); });
}

void TcpServer::removeConnection(TcpConnectionPtr conn)
{
    loop_->runInLoop([=] { removeConnectionInLoop(conn); });
}

void TcpServer::removeConnectionInLoop(TcpConnectionPtr conn)
{
    loop_->assertInLoopThread();
    LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_ << "] - connection " << conn->name();
    //_thread_fence_relaxed;
    size_t n = connections_.erase(conn->name());
    // if (n != 1)
    // {
    //     LOG_DEBUG << " fd=" << conn->get_fd() << ",n=" << n;
    // }
    (void)n;
    assert(n == 1);
    // EventLoop* ioLoop = conn->getLoop();
    // ioLoop->runInLoop([=] { conn->connectDestroyed(); });
}

void TcpServer::handle_sig_int()
{
    LOG_DEBUG << " ";
    acceptor_->stop();
    for (auto conn : connections_)
    {
        auto it = conn.second;
        it->forceClose();
        connections_.erase(conn.first);
    }

    threadPool_->stop();
    loop_->runInLoop([=] { loop_->quit_(); });
}
