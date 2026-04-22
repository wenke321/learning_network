
#include <functional>
#include <memory>

#include "InetAddr.h"

class Channel;
class EventLoop;

class Connector : public std::enable_shared_from_this<Connector>
{
   public:
    typedef std::function<void(int sockfd)> NewConnectionCallback;

    Connector(EventLoop* loop, const InetAddr& serverAddr);
    ~Connector();

    void setNewConnectionCallback(const NewConnectionCallback& cb) { newConnectionCallback_ = cb; }

    void start();    // can be called in any thread
    void restart();  // must be called in loop thread
    void stop();     // can be called in any thread

    const InetAddr& serverAddress() const { return serverAddr_; }

   private:
    enum States
    {
        Disconnected,
        Connecting,
        Connected
    };
    static const int MaxRetryDelayMs  = 30 * 1000;
    static const int InitRetryDelayMs = 500;

    void setState(States s) { state_ = s; }
    void startInLoop();
    void stopInLoop();
    void connect();
    bool checkConnect(int sockfd);
    void connecting(int sockfd);
    void handleWrite();
    void handleError();
    void retry(int sockfd);
    int removeAndResetChannel();
    void resetChannel();

    EventLoop* loop_;
    InetAddr serverAddr_;
    bool connect_;  // atomic
    States state_;  // FIXME: use atomic variable
    std::unique_ptr<Channel> channel_;
    NewConnectionCallback newConnectionCallback_;
    int retryDelayMs_;
};
