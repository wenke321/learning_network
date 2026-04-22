#pragma once

#include <fcntl.h>

#include <memory>
#include <string>
#include <vector>

#include "CountDownLatch.h"
#include "LogStream.h"
#include "Thread.h"
class AsyncLogger
{
   public:
    typedef FixedBuffer<4096 * 1024> LogBuffer;

    AsyncLogger(const std::string& basename_, off_t rollSize_, int flushInterval_);
    ~AsyncLogger();

    void start();
    void stop();

    void append(const char* logs, int len);

   private:
    void threadFunc();

    volatile bool running;  // atomic
    char pad[64 - sizeof(bool)];
    const off_t rollSize;
    const int flushInterval;
    const std::string basename;

    Thread thread;
    MutexLock mutex;
    Condition cond;
    CountDownLatch latch;
    std::unique_ptr<LogBuffer> cur_buffer;
    std::unique_ptr<LogBuffer> next_buffer;
    std::vector<std::unique_ptr<LogBuffer>> buffers;
};