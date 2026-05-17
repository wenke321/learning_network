#include <bits/types/sigset_t.h>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <vector>

#include "Buffer.h"

std::atomic<int> g_request_count{0};

#include <unistd.h>

#include "../src/Logger.h"
#include "../src/TcpServer.h"
#include "../src/helpers/kw_micros.h"
#include "AsyncLogger.h"
#include "CurrentThread.h"

int numThreads = 0;

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

class EchoServer
{
   public:
    EchoServer(EventLoop* loop, const InetAddr& listenAddr) : loop_(loop), server_(loop, listenAddr, "EchoServer")
    {
        server_.setConnectionCallback([this](TcpConnectionPtr conn) { onConnection(conn); });
        server_.setMessageCallback([this](TcpConnectionPtr conn, Buffer* buf) { onMessage(conn, buf); });
        server_.setThreadNum(numThreads);
    }

    void start() { server_.start(); }
    // void stop();

   private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        LOG_TRACE << conn->peerAddress()->ipPortStr() << " -> " << conn->localAddress()->ipPortStr() << " is " << (conn->connected() ? "UP" : "DOWN");
        // LOG_INFO << conn->getTcpInfoString();

        {
            LOG_DEBUG << " send quit,fd=" << conn->get_fd();
        }
        conn->send("quit\n");
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
            if (msg == "exit\n")
            {
                conn->send("bye\n");
                conn->shutdown();
            }
            if (msg == "quit\n")
            {
                loop_->quit_();
            }
            if (msg == "bye\n")
            {
                conn->send("yes\n");
                conn->set_unused();
            }
        }
        // if (msg == "world\n") conn->send("quit\n");

        // conn->send(msg);
    }

    EventLoop* loop_;
    TcpServer server_;
};

AsyncLogger asyncLog("Server", 10, 1);

void logoutput(const char* logs, int len) { asyncLog.append(logs, len); }

void qps_printer()
{
    while (true)
    {
        int last = g_request_count.exchange(0);
        printf("QPS: %d\n", last);
        sleep(1);
    }
}

int main(int argc, char* argv[])
{
    // Thread t(qps_printer, "qps");
    // t.start();
    Logger::setLogLevel(Logger::DEBUG);
    // asyncLog.start();
    // Logger::setOutput(logoutput);

    LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
    LOG_INFO << "sizeof TcpConnection = " << sizeof(TcpConnection);
    if (argc > 1)
    {
        numThreads = atoi(argv[1]);
    }

    EventLoop loop;  // acceptor
    InetAddr listenAddr(8080, false, 0);
    EchoServer server(&loop, listenAddr);

    server.start();

    loop.Loop();
    // t.join();
}
