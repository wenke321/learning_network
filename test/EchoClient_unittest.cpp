
#include <stdio.h>
#include <unistd.h>

#include <csignal>
#include <cstddef>

#include "AsyncLogger.h"
#include "CurrentThread.h"
#include "EventLoop.h"
#include "InetAddr.h"
#include "Logger.h"
#include "TcpClient.h"

int numThreads = 0;
class EchoClient;
std::vector<std::unique_ptr<EchoClient>> clients;
int current = 0;

std::vector<stringPiece> tcp_unpack(std::string& msg)
{
    std::vector<stringPiece> ret;
    unsigned long pos = 0, tag = 0;
    while (pos < msg.size())
    {
        tag = msg.find("\n", pos);
        ret.emplace_back(msg.data() + pos, tag - pos + 2);
        pos = tag + 2;
    }
    return ret;
}

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

    void a() { client_.handle_sig_int(); }

   private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        LOG_TRACE << conn->localAddress()->ipPortStr() << " -> " << conn->peerAddress()->ipPortStr() << " is " << (conn->connected() ? "UP" : "DOWN");

        if (conn->connected())
        {
            ++current;
            if (size_t(current) < clients.size())
            {
                clients[current]->connect();
            }
            LOG_INFO << "*** connected " << current;
        }
        // conn->send("exit\n");
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf)
    {
        std::string m(buf->retrieveAllAsString());
        std::vector<stringPiece> msgs = tcp_unpack(m);
        {
            LOG_DEBUG << conn->name() << " recv " << m.size() << " bytes : " << m << ",fd=" << conn->get_fd();
        }
        for (stringPiece msg : msgs)
        {
            if (msg == "quit\n")
            {
                conn->send("bye\n");
                conn->set_unused();
            }

            if (msg == "bye\n")
            {
                conn->set_unused();
            }

            if (msg == "yes\n")
            {
                conn->shutdown();
            }
        }
        // else
        // {
        //     // conn->send(msg);
        //     conn->send("world\n");
        // }
    }

    EventLoop* loop_;
    TcpClient client_;
};

AsyncLogger asyncLog("Client", 10, 1);

SignalHandler signal_handler;

void logoutput(const char* logs, int len) { asyncLog.append(logs, len); }

void handle_sig_int()
{
    LOG_DEBUG << " ";
    for (size_t i = 0; i < clients.size(); i++)
    {
        clients[i]->a();
    }
}

int main(int argc, char* argv[])
{
    Logger::setLogLevel(Logger::DEBUG);
    // asyncLog.start();
    // Logger::setOutput(logoutput);
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

        signal_handler.init(&loop);
        signal_handler.set_handle_int([&] { handle_sig_int(); });
        signal(SIGPIPE, SIG_IGN);

        loop.Loop();
    }
    else
    {
        printf("Usage: %s host_ip [current#]\n", argv[0]);
    }
}
