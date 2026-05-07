
#include "TcpClient.h"

#include <stdio.h>  // snprintf

#include <csignal>

#include "Connector.h"
#include "InetAddr.h"
#include "Logger.h"
#include "SignalHandler.h"
#include "Timer.h"
#include "Timestamp.h"

// TcpClient::TcpClient(EventLoop* loop)
//   : loop_(loop)
// {
// }

// TcpClient::TcpClient(EventLoop* loop, const string& host, uint16_t port)
//   : loop_(CHECK_NOTNULL(loop)),
//     serverAddr_(host, port)
// {
// }

void removeConnection(EventLoop* loop, const TcpConnectionPtr& conn)
{
    loop->queueInLoop([conn] { conn->connectDestroyed(); });
}
void removeConnector(const ConnectorPtr& connector)
{
    while (connector.use_count() > 1) sched_yield();
}

TcpClient::TcpClient(EventLoop* loop, const InetAddr& serverAddr, const std::string& nameArg) : loop_(loop), connector_(new Connector(loop, serverAddr)), name_(nameArg), connectionCallback_(defaultConnectionCallback), messageCallback_(defaultMessageCallback), retry_(false), connect_(true), nextConnId_(1)
{
    connector_->setNewConnectionCallback([this](int fd) { newConnection(fd); });
    // FIXME setConnectFailedCallback
    LOG_INFO << "TcpClient::TcpClient[" << name_ << "] - connector " << connector_.get();
}

TcpClient::~TcpClient()
{
    LOG_INFO << "TcpClient::~TcpClient[" << name_ << "] - connector " << connector_.get();
    TcpConnectionPtr conn;
    bool unique = false;
    {
        MutexLockGuard lock(mutex_);
        unique = connection_.unique();
        conn   = connection_;
    }
    if (conn)
    {
        assert(loop_ == conn->getLoop());
        // FIXME: not 100% safe, if we are in different thread
        CloseCallback cb = [this](const TcpConnectionPtr& conn) { ::removeConnection(loop_, conn); };
        loop_->runInLoop(std::bind(&TcpConnection::setCloseCallback, conn, cb));
        if (unique)
        {
            conn->forceClose();
        }
    }
    else
    {
        connector_->stop();
        // FIXME: HACK
        Timer* t = new Timer(Timestamp::now_microsecconds() + 1000, [this] { ::removeConnector(connector_); });
        loop_->addTimer(t);
    }
}

void TcpClient::connect()
{
    // FIXME: check state
    LOG_INFO << "TcpClient::connect[" << name_ << "] - connecting to " << connector_->serverAddress().ipPortStr();
    connect_ = true;
    connector_->start();
}

void TcpClient::disconnect()
{
    connect_ = false;

    {
        MutexLockGuard lock(mutex_);
        if (connection_)
        {
            connection_->shutdown();
        }
    }
}

void TcpClient::stop()
{
    connect_ = false;
    connector_->stop();
}

void TcpClient::newConnection(int sockfd)
{
    LOG_TRACE << "TcpClient::newConnection";
    loop_->assertInLoopThread();
    InetAddr* peerAddr = new InetAddr(sockOption::getPeerAddr(sockfd));
    char buf[32];
    snprintf(buf, sizeof buf, ":%s#%d", peerAddr->ipPortStr().c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    InetAddr* localAddr = new InetAddr(sockOption::getLocalAddr(sockfd));
    // FIXME poll with zero timeout to double confirm the new connection
    // FIXME use make_shared if necessary
    TcpConnectionPtr conn(new TcpConnection(connector_->ch_(), connName, localAddr, peerAddr));

    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback([this](TcpConnectionPtr conn) { removeConnection(conn); });  // FIXME: unsafe
    {
        MutexLockGuard lock(mutex_);
        connection_ = conn;
    }
    conn->connectEstablished();
}

void TcpClient::removeConnection(TcpConnectionPtr conn)
{
    LOG_TRACE << "TcpClient::removeConnection";
    loop_->assertInLoopThread();
    assert(loop_ == conn->getLoop());

    {
        MutexLockGuard lock(mutex_);
        if (connection_)
            assert(connection_ == conn);
        else
        {
            LOG_ERROR << " TcpClient::removeConnection,connection_=null,should never happen";
        }
        connection_.reset();
    }

    loop_->runInLoop([conn_ = conn] { conn_->connectDestroyed(); });
    if (retry_ && connect_)
    {
        LOG_INFO << "TcpClient::connect[" << name_ << "] - Reconnecting to " << connector_->serverAddress().ipPortStr();
        connector_->restart();
    }
}

void TcpClient::handle_sig_int()
{
    LOG_DEBUG << " ";
    if (connection_)
    {
        LOG_DEBUG << " ";
        connection_->shutdown();
    }
    connect_ = false;
    // loop_->quit_();
}
