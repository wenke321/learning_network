
#include <stdio.h>
#include <unistd.h>

#include <utility>

#include "CurrentThread.h"
#include "EventLoop.h"
#include "InetAddr.h"
#include "Logger.h"
#include "TcpClient.h"
#include "Timestamp.h"

int numThreads = 0;
class EchoClient;
std::vector<std::unique_ptr<EchoClient>> clients;
int current = 0;

class EchoClient
{
   public:
    EchoClient(EventLoop* loop, const InetAddr& listenAddr, const std::string& id) : loop_(loop), client_(loop, listenAddr, "EchoClient" + id)
    {
        client_.setConnectionCallback(std::bind(&EchoClient::onConnection, this, std::placeholders::_1));
        client_.setMessageCallback(std::bind(&EchoClient::onMessage, this, std::placeholders::_1, std::placeholders::_2));
        // client_.enableRetry();
    }

    void connect() { client_.connect(); }
    // void stop();

   private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        LOG_TRACE << conn->localAddress().ipPortStr() << " -> " << conn->peerAddress().ipPortStr() << " is " << (conn->connected() ? "UP" : "DOWN");

        if (conn->connected())
        {
            ++current;
            if (size_t(current) < clients.size())
            {
                clients[current]->connect();
            }
            LOG_INFO << "*** connected " << current;
        }
        conn->send("world\n");
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf)
    {
        std::string msg(buf->retrieveAllAsString());
        LOG_TRACE << conn->name() << " recv " << msg.size() << " bytes at " << Timestamp::now().toStr();
        if (msg == "quit\n")
        {
            conn->send("bye\n");
            conn->shutdown();
        }
        else if (msg == "shutdown\n")
        {
            loop_->quit_();
        }
        else
        {
            conn->send(msg);
        }
    }

    EventLoop* loop_;
    TcpClient client_;
};

int main(int argc, char* argv[])
{
    Logger::setLogLevel(Logger::INFO);
    LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
    if (argc > 1)
    {
        EventLoop loop;
        bool ipv6 = argc > 3;
        InetAddr serverAddr(argv[1], 8080, ipv6);

        int n = 1;
        if (argc > 2)
        {
            n = atoi(argv[2]);
        }

        clients.reserve(n);
        for (int i = 0; i < n; ++i)
        {
            char buf[32];
            snprintf(buf, sizeof buf, "%d", i + 1);
            clients.emplace_back(new EchoClient(&loop, serverAddr, buf));
        }

        clients[current]->connect();
        loop.Loop();
    }
    else
    {
        printf("Usage: %s host_ip [current#]\n", argv[0]);
    }
}
