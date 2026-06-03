
#include "TcpClient.h"

#include <stdio.h>  // snprintf

#include <csignal>
#include <memory>

#include "Connector.h"
#include "Loggers/Logger.h"
#include "Sockets/InetAddr.h"
#include "Timers/Timer.h"
#include "Timers/Timestamp.h"
#include "https/ssl_TcpConnection.h"
#include "https/ssl_context.h"
#include "https/ssl_handshake_handler.h"

// TcpClient::TcpClient(EventLoop* loop)
//   : loop_(loop)
// {
// }

// TcpClient::TcpClient(EventLoop* loop, const string& host, uint16_t port)
//   : loop_(CHECK_NOTNULL(loop)),
//     serverAddr_(host, port)
// {
// }

// void removeConnection(EventLoop* loop, const TcpConnectionPtr& conn)
// {
//     loop->queueInLoop([conn] { conn->connectDestroyed(); });
// }
void removeConnector(const ConnectorPtr& connector)
{
    while (connector.use_count() > 1) sched_yield();
}

TcpClient::TcpClient(EventLoop* loop, const InetAddr& serverAddr, const std::string& nameArg, bool _keep_alive) : loop_(loop), connector_(new Connector(loop, serverAddr)), name_(nameArg), connectionCallback_(defaultConnectionCallback), messageCallback_(defaultMessageCallback), retry_(false), connect_(false), keep_alive(_keep_alive), nextConnId_(1)
{
    connector_->setNewConnectionCallback([this](int fd) { newConnection(fd); });
    // FIXME setConnectFailedCallback
    {
        LOG_INFO << "TcpClient::TcpClient[" << name_ << "] - connector " << connector_.get();
    }
}

TcpClient::TcpClient(EventLoop* loop, const std::string& nameArg) : loop_(loop), name_(nameArg), retry_(false), connect_(false), nextConnId_(1) {}

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
        // CloseCallback cb = [this](const TcpConnectionPtr& conn) { ::removeConnection(loop_, conn); };
        // loop_->runInLoop(std::bind(&TcpConnection::setCloseCallback, conn, cb));
        // if (unique)
        // {
        //     conn->forceClose();
        // }
    }
    else
    {
        connector_->stop();
        loop_->runAfter(Timestamp::now_microsecconds() + 1000, [this] { ::removeConnector(connector_); });
    }
}

void TcpClient::enbale_ssl(std::shared_ptr<ssl_context> _ssl_ctx, std::string& host)
{
    ssl_ctx      = _ssl_ctx;
    sslHostname_ = host;
    enable_ssl_  = true;
}

bool TcpClient::get_connect() { return connect_; }

void TcpClient::connect()
{
    // FIXME: check state
    {
        LOG_INFO << "TcpClient::connect[" << name_ << "] - connecting to " << connector_->serverAddress().ipPortStr();
    }
    connect_ = true;
    connector_->start();
}

void TcpClient::force_close() { connection_->forceClose(); }

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
    {
        LOG_DEBUG << " ";
    }
    loop_->assertInLoopThread();
    InetAddr* peerAddr = new InetAddr(sockOption::getPeerAddr(sockfd));
    char buf[32];
    snprintf(buf, sizeof buf, ":%s#%d", peerAddr->ipPortStr().c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    InetAddr* localAddr = new InetAddr(sockOption::getLocalAddr(sockfd));

    if (enable_ssl_)
    {
        SSL* ssl = ssl_ctx->create_SSL(sockfd);
        if (!sslHostname_.empty())
        {
            SSL_set_tlsext_host_name(ssl, sslHostname_.c_str());  // SNI
            SSL_set1_host(ssl, sslHostname_.c_str());             // 证书主机名检查
        }

        handler = std::make_shared<ssl_handshake_handler>(
            loop_, connector_->ch_(), ssl,
            // 握手成功回调
            [=](SSL* ssl, int fd)
            {
                // 创建支持 SSL 的 TcpConnection（见后文）
                TcpConnectionPtr conn = std::make_shared<ssl_TcpConnection>(connector_->ch_(), connName, localAddr, peerAddr, keep_alive, ssl);
                connector_->resetChannel();
                conn->setConnectionCallback(connectionCallback_);
                conn->setMessageCallback(messageCallback_);
                conn->setWriteCompleteCallback(writeCompleteCallback_);
                conn->setCloseCallback([this](TcpConnectionPtr conn) { removeConnection(conn); });
                {
                    MutexLockGuard lock(mutex_);
                    connection_ = conn;
                }

                conn->connectEstablished();  // 触发 Channel 事件监听
            },
            // 握手失败回调
            [this](int sockfd)
            {
                {
                    LOG_WARN << "SSL handshake failed,fd=" << sockfd;
                }
                // 可以在这里重试，但需要确保不违反 retry_ 策略
                // 简单起见直接关闭，并通知上层（可通过回调）
                if (retry_ && connect_)
                {
                    loop_->runAfter(1000, [&] { connect(); });
                    retry_ = false;
                }
            });
        handler->start();
    }
    else
    {
        // FIXME poll with zero timeout to double confirm the new connection
        TcpConnectionPtr conn = std::make_shared<TcpConnection>(connector_->ch_(), connName, localAddr, peerAddr, keep_alive);
        connector_->resetChannel();
        conn->setConnectionCallback(connectionCallback_);
        conn->setMessageCallback(messageCallback_);
        conn->setWriteCompleteCallback(writeCompleteCallback_);
        conn->setCloseCallback([this](TcpConnectionPtr conn) { removeConnection(conn); });
        {
            MutexLockGuard lock(mutex_);
            connection_ = conn;
        }

        conn->connectEstablished();
    }
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

const std::string& TcpClient::name() const { return name_; }

EventLoop* TcpClient::getLoop() const { return loop_; }

bool TcpClient::retry() const { return retry_; }
void TcpClient::enableRetry() { retry_ = true; }

void TcpClient::setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }

void TcpClient::setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }

void TcpClient::set_OOB_callback(const std::function<void(const TcpConnectionPtr&, char)>& _cb) { OOB_callback = _cb; }

void TcpClient::setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); }

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

void TcpClient::enable_ssl(std::shared_ptr<ssl_context> ctx, const std::string& hostname = "")
{
    ssl_ctx      = std::move(ctx);
    sslHostname_ = hostname;
    enable_ssl_  = true;
}
