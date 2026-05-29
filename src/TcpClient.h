

#include <openssl/ssl.h>

#include <memory>
#include <string>

#include "EventLoop.h"
#include "SignalHandler.h"
#include "Sockets/InetAddr.h"
#include "TcpConnection.h"
#include "https/ssl_context.h"
#include "https/ssl_handshake_handler.h"

class Connector;
typedef std::shared_ptr<Connector> ConnectorPtr;

class TcpClient
{
   public:
    // TcpClient(EventLoop* loop);
    // TcpClient(EventLoop* loop, const string& host, uint16_t port);
    TcpClient(EventLoop* loop, const InetAddr& serverAddr, const std::string& nameArg, bool _keep_alive);
    TcpClient(EventLoop* loop, const std::string& nameArg);
    ~TcpClient();  // force out-line dtor, for std::unique_ptr members.

    void connect();
    void disconnect();
    void stop();

    TcpConnectionPtr connection() const
    {
        MutexLockGuard lock(mutex_);
        return connection_;
    }

    void enbale_ssl(std::shared_ptr<ssl_context> ssl_ctx, std::string& host);
    // void set_serv_addr(std::string& _addr);

    EventLoop* getLoop() const;
    bool retry() const;
    void enableRetry();

    const std::string& name() const;

    /// Set connection callback.
    /// Not thread safe.
    void setConnectionCallback(ConnectionCallback cb);

    /// Set message callback.
    /// Not thread safe.
    void setMessageCallback(MessageCallback cb);

    void set_OOB_callback(const std::function<void(const TcpConnectionPtr&, char)>& _cb);

    /// Set write complete callback.
    /// Not thread safe.
    void setWriteCompleteCallback(WriteCompleteCallback cb);

    void handle_sig_int();

   private:
    /// Not thread safe, but in loop
    void newConnection(int sockfd);
    /// Not thread safe, but in loop
    void removeConnection(TcpConnectionPtr conn);

    EventLoop* loop_;
    ConnectorPtr connector_;  // avoid revealing Connector
    const std::string name_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    std::function<void(const TcpConnectionPtr&, char)> OOB_callback;
    WriteCompleteCallback writeCompleteCallback_;
    bool retry_;    // atomic
    bool connect_;  // atomic
    bool keep_alive;
    // always in loop thread
    int nextConnId_;
    mutable MutexLock mutex_;
    TcpConnectionPtr connection_;

    // ssl
    void enable_ssl(std::shared_ptr<ssl_context> ctx, const std::string& hostname);
    std::shared_ptr<ssl_handshake_handler> handler;
    std::shared_ptr<ssl_context> ssl_ctx;
    std::string sslHostname_;
    bool enable_ssl_;
};
