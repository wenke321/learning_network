#include <stdio.h>
#include <unistd.h>

#include "EventLoop.h"
#include "EventloopThread.h"
#include "Loggers/AsyncLogger.h"
#include "Loggers/Logger.h"
#include "Timers/Timestamp.h"

int cnt = 0;
EventLoop* g_loop;

void default_cb_(EventLoop* l) {}

void printTid()
{
    printf("pid = %d, tid = %d\n", getpid(), CurrentThread::tid());
    printf("now %s\n", Timestamp::now().toStr().c_str());
}

void print(const char* msg)
{
    printf("msg %s %s\n", Timestamp::now().toStr().c_str(), msg);
    if (++cnt == 20)
    {
        g_loop->quit_();
    }
}

void cancel(Timer* timer)
{
    g_loop->cancelTimer(timer);
    printf("cancelled at %s\n", Timestamp::now().toStr().c_str());
}

AsyncLogger* log = new AsyncLogger("timer_test", 10, 3);
void f(const char* logs, int len) { log->append(logs, len); }

int main()
{
    // log->start();
    Logger::setLogLevel(Logger::DEBUG);
    // Logger::setOutput(f);
    printTid();
    sleep(1);

    EventLoop loop;
    g_loop = &loop;

    print("main");
    loop.runAfter(1, [] { print("once1"); });
    loop.runAfter(1.5, [] { print("once1.5"); });
    loop.runAfter(2.5, [] { print("once2.5"); });
    loop.runAfter(3.5, [] { print("once3.5"); });
    Timer t45(Timestamp::now_microsecconds() + 4.5 * 1000000, [] { print("once4.5"); });
    loop.addTimer(&t45);
    loop.runAfter(4.2, [&] { cancel(&t45); });
    loop.runAfter(4.8, [&] { cancel(&t45); });
    loop.runEvery([] { print("every2"); }, 2);
    Timer t3(Timestamp::now_microsecconds(), [] { print("every3"); }, 3);
    loop.addTimer(&t3);
    loop.runAfter(9.001, [&] { cancel(&t3); });
    loop.runAfter(10, [&] { loop.quit_(); });

    loop.Loop();
    print("main loop exits");

    sleep(1);
    {
        EventloopThread loopThread("timer", default_cb_);
        EventLoop* loop = loopThread.startLoop();
        loop->runAfter(2, printTid);
        sleep(3);
        print("thread loop exits");
    }
}