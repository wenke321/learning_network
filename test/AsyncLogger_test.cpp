#include "Loggers/AsyncLogger.h"

#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>

#include <cstddef>

#include "Loggers/Logger.h"
#include "Threads/ThreadPool.h"
#include "Timers/Timestamp.h"

off_t kRollSize = 500 * 1000 * 1000;

__thread double start = 0;
__thread double end   = 0;

AsyncLogger* g_asyncLog = NULL;

void asyncOutput(const char* msg, int len) { g_asyncLog->append(msg, len); }

void bench(bool longLog)
{
    Logger::setOutput(asyncOutput);

    int cnt           = 0;
    const int kBatch  = 1000;
    std::string empty = " ";
    std::string longStr(1000, 'X');
    longStr += " ";

    for (int t = 0; t < 30; ++t)
    {
        start = Timestamp::now_microsecconds();
        for (int i = 0; i < kBatch; ++i)
        {
            LOG_INFO << "Hello 0123456789" << " abcdefghijklmnopqrstuvwxyz " << (longLog ? longStr : empty);
            ++cnt;
        }
        end = Timestamp::now_microsecconds();
        printf("%f\n", (end - start) / 1000000 / kBatch);
        // struct timespec ts = {0, 500 * 1000 * 1000};
        // nanosleep(&ts, NULL);
    }
}

int main(int argc, char* argv[])
{
    {
        // set max virtual memory to 2GB.
        size_t kOneGB = 1000 * 1024 * 1024;
        rlimit rl     = {2 * kOneGB, 2 * kOneGB};
        setrlimit(RLIMIT_AS, &rl);
    }

    printf("pid = %d\n", getpid());

    char name[256] = {'\0'};
    strncpy(name, argv[0], sizeof name - 1);
    AsyncLogger log(::basename(name), 10, 3);
    log.start();
    g_asyncLog = &log;

    bool longLog = argc > 1;
    ThreadPool pool(4, "loging", 4);
    pool.start();
    for (int i = 0; i < 4; i++) pool.submit([=] { bench(longLog); });
}