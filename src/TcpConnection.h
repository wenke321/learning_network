
#pragma once
#include <openssl/crypto.h>

#include <forward_list>
#include <memory>
#include <string>

#include "EventLoop.h"
#include "Memorys/Buffer.h"
#include "Sockets/InetAddr.h"
#include "basics/Channel.h"
#include "helpers/type_traits.h"

#define senders_offset 6

class TcpConnection;
typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;

typedef std::function<void()> TimerCallback;
typedef std::function<void(const TcpConnectionPtr&)> ConnectionCallback;
typedef std::function<void(const TcpConnectionPtr&)> CloseCallback;
typedef std::function<void(const TcpConnectionPtr&)> WriteCompleteCallback;
typedef std::function<void(const TcpConnectionPtr&, size_t)> HighWaterMarkCallback;

// the data has been read to (buf, len)
typedef std::function<void(const TcpConnectionPtr&, Buffer*)> MessageCallback;

void defaultConnectionCallback(const TcpConnectionPtr& conn);
void defaultMessageCallback(const TcpConnectionPtr& conn, Buffer* buffer);

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
   public:
    /// Constructs a TcpConnection with a connected sockfd
    ///
    /// User should not create this object.
    explicit TcpConnection(EventLoop* loop, const std::string& name, int sockfd, const InetAddr* localAddr, const InetAddr* peerAddr, bool _keep_alive);
    explicit TcpConnection(Channel*&, const std::string& name, const InetAddr* localAddr, const InetAddr* peerAddr, bool _keep_alive);
    virtual ~TcpConnection();

    EventLoop* getLoop() const;
    const std::string& name() const;

    const InetAddr* localAddress() const;
    const InetAddr* peerAddress() const;

    bool connected() const;
    bool disconnected() const;
    // return true if success.
    bool getTcpInfo(struct tcp_info*) const;
    std::string getTcpInfoString() const;

    // void send(string&& message);
    void send(const void* message, int len);
    void send(const stringPiece& message);
    // void send(Buffer&& message);
    void send(Buffer* message);  // this one will swap data

    void send_OOB(const stringPiece& message);

    void shutdown();  // thread safe
    void shutdown_write();
    // void shutdownAndForceCloseAfter(double seconds); // NOT thread safe, no simultaneous calling
    void forceClose();
    void forceCloseWithDelay(double seconds);

    void setTcpNoDelay(bool on);
    // reading or not
    void startRead();
    void stopRead();
    bool isReading() const { return reading_; };  // NOT thread safe, may race with start/stopReadInLoop

    // void setContext(const boost::any& context) { context_ = context; }

    // const boost::any& getContext() const { return context_; }

    // boost::any* getMutableContext() { return &context_; }

    void setConnectionCallback(const ConnectionCallback& cb);
    void setMessageCallback(const MessageCallback& cb);
    void set_OOB_callback(std::function<void(const TcpConnectionPtr&, char)>);
    void setWriteCompleteCallback(const WriteCompleteCallback& cb);
    void setCloseCallback(const CloseCallback& cb);
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark);

    /// Advanced interface
    Buffer* inputBuffer() { return &inputBuffer_; }

    std::forward_list<Buffer>& outputBuffer() { return outputBuffers_; }

    /// Internal use only.

    // called when TcpServer accepts a new connection
    void connectEstablished();  // should be called only once
    // called when TcpServer has removed me from its map
    void connectDestroyed();  // should be called only once

    // it means you don't need this connection any more
    void set_unused();

    int get_fd();

    enum StateE
    {
        Connecting    = 1,
        Disconnecting = 2,
        Disconnected  = 4,
        Connected     = 8,
        used          = 16,
        writing       = 32,
    };

   private:
    Buffer inputBuffer_;
    std::forward_list<Buffer> outputBuffers_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    std::function<void(const TcpConnectionPtr&, char)> OOB_messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;

    volatile atomic_ulong state_;
    char pad1[cache_line_size - sizeof(state_)];

    const std::string name_;

    bool reading_;
    bool rdhup_phrase;

    std::unique_ptr<Socket> socket_;
    Channel* channel_;
    const InetAddr* localAddr_;
    const InetAddr* peerAddr_;

    size_t highWaterMark_;

    // virtual for ssl_TcpConnection
    void handle_ep_pri();
    virtual void handle_ep_in();
    virtual void handle_ep_out();
    virtual bool finish_in_out();
    void handle_ep_rdhup();
    virtual void handle_ep_hup();
    void handle_ep_err();

    // void sendInLoop(string&& message);
    void sendInLoop(const stringPiece message);
    //  virtual for ssl_TcpConnection
    virtual void sendInLoop(const void* message, size_t len);

    void shutdownInLoop();
    // void shutdownAndForceCloseInLoop(double seconds);
    void forceCloseInLoop();

    void setState(StateE s) { state_ = s; }
    std::string stateToString(int) const;

    void startReadInLoop();
    void stopReadInLoop();

    EventLoop* loop_;

    size_t oldLen = 0;

    SSL* ssl_;
    friend class ssl_TcpConnection;
    //  FIXME: creationTime_, lastReceiveTime_
    //         bytesReceived_, bytesSent_
};
