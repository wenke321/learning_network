#include "AsyncLogger.h"

#include <cassert>
#include <memory>
#include <utility>
#include <vector>

#include "CountDownLatch.h"
#include "LogFile.h"
#include "Thread.h"

AsyncLogger::AsyncLogger(const std::string& basename_, off_t rollSize_, int flushInterval_) : running(0), rollSize(rollSize_), flushInterval(flushInterval_), basename(basename_), thread([&] { threadFunc(); }, "Logging"), mutex(), cond(mutex), latch(1)
{
    cur_buffer  = std::make_unique<LogBuffer>();
    next_buffer = std::make_unique<LogBuffer>();
}

AsyncLogger::~AsyncLogger()
{
    if (running)
    {
        stop();
    }
}

void AsyncLogger::start()
{
    running = true;
    thread.start();
    latch.wait();
}

void AsyncLogger::stop()
{
    running = false;
    cond.notify();
    thread.join();
}

void AsyncLogger::append(const char* logs, int len)
{
    MutexLockGuard lock(mutex);
    if (cur_buffer->avail() >= len)
    {
        cur_buffer->append(logs, len);
    }
    else
    {
        buffers.push_back(std::move(cur_buffer));
        if (next_buffer)
        {
            cur_buffer = std::move(next_buffer);
        }
        else
        {
            cur_buffer.reset(new LogBuffer);
        }

        cur_buffer->append(logs, len);
        cond.notify();
    }
}

void AsyncLogger::threadFunc()
{
    assert(running == true);
    latch.countDown();
    std::unique_ptr<LogBuffer> newBuffer1(new LogBuffer);
    std::unique_ptr<LogBuffer> newBuffer2(new LogBuffer);
    LogFile log_file(basename, rollSize, flushInterval, 1024, false);
    std::vector<std::unique_ptr<LogBuffer>> buffersToWrite;
    newBuffer1->bzero();
    newBuffer2->bzero();

    while (running)
    {
        assert(newBuffer1 && newBuffer1->length() == 0);
        assert(newBuffer2 && newBuffer2->length() == 0);
        assert(buffersToWrite.empty());

        {
            MutexLockGuard lock(mutex);
            if (buffers.empty())
            {
                cond.wait();
            }
            buffers.push_back(std::move(cur_buffer));
            cur_buffer = std::move(newBuffer1);
            buffersToWrite.swap(buffers);
            if (!next_buffer)
            {
                next_buffer = std::move(newBuffer2);
            }
        }

        for (std::unique_ptr<LogBuffer>& buf : buffersToWrite)
        {
            log_file.append(buf->data_(), buf->length());
        }

        newBuffer1 = std::move(buffersToWrite.back());
        buffersToWrite.pop_back();
        newBuffer1->reset();

        if (!newBuffer2)
        {
            if (buffersToWrite.size())
            {
                newBuffer2 = std::move(buffersToWrite.back());
                buffersToWrite.pop_back();
                newBuffer2->reset();
            }
            else
                newBuffer2 = std::make_unique<LogBuffer>();
        }

        buffersToWrite.clear();
        log_file.flush();
    }

    log_file.flush();
}