
#include "TcpServer.h"

#include <cassert>
#include <csignal>
#include <memory>

#include "Acceptor.h"
#include "Channel.h"
#include "EventLoop.h"
#include "InetAddr.h"
#include "Logger.h"
#include "SignalHandler.h"
#include "TcpConnection.h"

TcpServer::TcpServer(EventLoop* loop, const InetAddr& listenAddr, const std::string& nameArg, Option option) : loop_(loop), ipPort_(listenAddr.ipPortStr()), name_(nameArg), threadPool_(new EventloopThreadPool(loop, name_)), connectionCallback_(defaultConnectionCallback), messageCallback_(defaultMessageCallback), nextConnId_(1), acceptor_(new Acceptor(loop, listenAddr, option == ReusePort)), signal_handler()
{
    acceptor_->setNewConnectionCallback([=](int sockfd, const InetAddr* peerAddr_) { newConnection(sockfd, peerAddr_); });

    signal_handler.init(loop_, [=] { handle_signal(); });
    signal(SIGPIPE, SIG_IGN);
}

TcpServer::~TcpServer()
{
    loop_->assertInLoopThread();
    LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";

    for (auto& item : connections_)
    {
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        conn->getLoop()->runInLoop([=] { conn->connectDestroyed(); });
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

        Channel* ch = signal_handler.get_ch();
        for (auto l : threadPool_->get_ioloops())
        {
            // l->runInLoop([&] { l->add_channel(ch); });
            ch->setIndex(Channel::ch_extern);
            l->add_channel(ch);
        }

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

    LOG_INFO << "TcpServer::newConnection [" << name_ << "] - new connection [" << connName << "] from " << peerAddr->ipPortStr();
    // FIXME poll with zero timeout to double confirm the new connection
    // FIXME use make_shared if necessary
    InetAddr* localAddr = new InetAddr(sockOption::getLocalAddr(sockfd));
    ioLoop->runInLoop(
        [=]
        {
            TcpConnectionPtr conn = std::make_shared<TcpConnection>(ioLoop, connName, sockfd, localAddr, peerAddr);

            conn->setConnectionCallback(connectionCallback_);
            conn->setMessageCallback(messageCallback_);
            conn->setWriteCompleteCallback(writeCompleteCallback_);
            conn->setCloseCallback([this](TcpConnectionPtr _conn) { removeConnection(_conn); });
            loop_->runInLoop(
                [=]
                {
                    connections_[connName] = conn;
                    conn->connectEstablished();
                });
        });
}

void TcpServer::removeConnection(TcpConnectionPtr conn)
{
    loop_->runInLoop([=] { removeConnectionInLoop(conn); });
}

void TcpServer::removeConnectionInLoop(TcpConnectionPtr conn)
{
    loop_->assertInLoopThread();
    LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_ << "] - connection " << conn->name();
    size_t n = connections_.erase(conn->name());
    (void)n;
    assert(n == 1);
    // EventLoop* ioLoop = conn->getLoop();
    // ioLoop->runInLoop([=] { conn->connectDestroyed(); });
}

void TcpServer::handle_signal()
{
    LOG_DEBUG << " ";
    acceptor_->stop();
    for (auto conn : connections_)
    {
        auto it = conn.second;
        it->shutdown();
        it->getLoop()->runInLoop([=] { it->connectDestroyed(); });
    }

    for (auto l : threadPool_->get_ioloops()) l->quit_();
    loop_->runInLoop([=] { loop_->quit_(); });
}
