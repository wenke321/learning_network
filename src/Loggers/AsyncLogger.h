#pragma once

#include <fcntl.h>

#include <memory>
#include <string>
#include <vector>

#include "../Threads/CountDownLatch.h"
#include "../Threads/Thread.h"
#include "../helpers/type_traits.h"
#include "LogStream.h"

#define appending 1
#define full      2
#define swaping   4
#define available 8
#define stoping   16

class AsyncLogger
{
   public:
    typedef FixedBuffer<1024 * 1024> LogBuffer;

    AsyncLogger(const std::string& basename_, off_t rollSize_, int flushInterval_);
    ~AsyncLogger();

    void start();
    void stop();

    void append(const char* logs, int len);

   private:
    void threadFunc();

    volatile bool running;  // atomic
    char pad[64 - sizeof(bool)];
    atomic_ulong state;
    char pad1[64 - sizeof(state)];
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