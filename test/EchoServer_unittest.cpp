#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

std::atomic<int> g_request_count{0};

#include <unistd.h>

#include <iostream>

#include "../src/Logger.h"
#include "../src/TcpServer.h"
#include "AsyncLogger.h"
#include "CurrentThread.h"

int numThreads = 0;

class EchoServer
{
   public:
    EchoServer(EventLoop* loop, const InetAddr& listenAddr) : loop_(loop), server_(loop, listenAddr, "EchoServer")
    {
        server_.setConnectionCallback([this](const TcpConnectionPtr& conn) { onConnection(conn); });
        server_.setMessageCallback([this](const TcpConnectionPtr& conn, Buffer* buf) { onMessage(conn, buf); });
        server_.setThreadNum(numThreads);
    }

    void start() { server_.start(); }
    // void stop();

   private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        LOG_TRACE << conn->peerAddress().ipPortStr() << " -> " << conn->localAddress().ipPortStr() << " is " << (conn->connected() ? "UP" : "DOWN");
        LOG_INFO << conn->getTcpInfoString();

        g_request_count++;
        conn->send("hello\n");
        conn->send("quit\n");
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf)
    {
        std::string msg(buf->retrieveAllAsString());
        LOG_TRACE << conn->name() << " recv " << msg.size() << " bytes at " << Timestamp::now().toStr();
        if (msg == "exit\n")
        {
            conn->send("bye\n");
            conn->shutdown();
        }
        if (msg == "quit\n")
        {
            loop_->quit_();
        }

        conn->send(msg);
    }

    EventLoop* loop_;
    TcpServer server_;
};

AsyncLogger asyncLog("Server ", 32, 3);

void logoutput(const char* logs, int len) { asyncLog.append(logs, len); }

void qps_printer()
{
    while (true)
    {
        int last = g_request_count.exchange(0);
        printf("QPS: %d\n", last);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main(int argc, char* argv[])
{
    // std::thread(qps_printer).detach();
    Logger::setLogLevel(Logger::INFO);
    // asyncLog.start();
    // Logger::setOutput(logoutput);

    LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
    LOG_INFO << "sizeof TcpConnection = " << sizeof(TcpConnection);
    if (argc > 1)
    {
        numThreads = atoi(argv[1]);
    }

    EventLoop loop;
    InetAddr listenAddr(8080, false, 0);
    EchoServer server(&loop, listenAddr);

    server.start();

    loop.Loop();
}
