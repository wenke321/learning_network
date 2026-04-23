
#include "TcpServer.h"

#include <cassert>
#include <memory>

#include "Acceptor.h"
#include "EventLoop.h"
#include "InetAddr.h"
#include "Logger.h"
#include "TcpConnection.h"

TcpServer::TcpServer(EventLoop* loop, const InetAddr& listenAddr, const std::string& nameArg, Option option) : loop_(loop), ipPort_(listenAddr.ipPortStr()), name_(nameArg), acceptor_(new Acceptor(loop, listenAddr, option == ReusePort)), threadPool_(new EventloopThreadPool(loop, name_)), connectionCallback_(defaultConnectionCallback), messageCallback_(defaultMessageCallback), nextConnId_(1)
{
    acceptor_->setNewConnectionCallback([this](int sockfd, const InetAddr& peerAddr_) { newConnection(sockfd, peerAddr_); });
}

TcpServer::~TcpServer()
{
    loop_->assertInLoopThread();
    LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";

    for (auto& item : connections_)
    {
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        conn->getLoop()->runInLoop([conn_ = conn] { conn_->connectDestroyed(); });
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

void TcpServer::newConnection(int sockfd, InetAddr peerAddr)
{
    LOG_TRACE << "TcpServer::newConnection";
    loop_->assertInLoopThread();
    EventLoop* ioLoop = threadPool_->getNextLoop();
    char buf[64];
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO << "TcpServer::newConnection [" << name_ << "] - new connection [" << connName << "] from " << peerAddr.ipPortStr();
    InetAddr localAddr(sockOption::getLocalAddr(sockfd));
    // FIXME poll with zero timeout to double confirm the new connection
    // FIXME use make_shared if necessary
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback([this](const std::shared_ptr<TcpConnection>& conn) { removeConnection(conn); });  // FIXME: unsafe
    ioLoop->runInLoop([conn_ = conn] { conn_->connectEstablished(); });
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    // FIXME: unsafe
    loop_->runInLoop([this, conn_ = conn] { removeConnectionInLoop(conn_); });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    loop_->assertInLoopThread();
    LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_ << "] - connection " << conn->name();
    size_t n = connections_.erase(conn->name());
    (void)n;
    assert(n == 1);
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop([conn_ = conn] { conn_->connectDestroyed(); });
}
