#include "AsyncLogger.h"

#include <cassert>
#include <climits>
#include <memory>
#include <utility>
#include <vector>

#include "../Threads/CountDownLatch.h"
#include "../Threads/Thread.h"
#include "../helpers/builtins.h"
#include "../helpers/kw_micros.h"
#include "LogFile.h"
#include "Logger.h"

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
    LOG_INFO << "AsyncLogger::start";
}

void AsyncLogger::stop()
{
    running = false;

    for (;;)
    {
        atomic_ulong old_state = _load_acquire(&state);

        if (!(old_state & available))
        {
            {
                LOG_DEBUG << " wrong state,should sync";
            }
            return;
        }

        if (old_state & (full | appending | swaping))
        {
            futex_wait(&state, old_state);
            continue;
        }

        if (!_CAS_strong_release(&state, &old_state, 0)) continue;
        futex_wake(&state);
        break;
    }

    thread.join();
}

void AsyncLogger::append(const char* logs, int len)
{
    for (;;)
    {
        atomic_ulong old_state = _load_acquire(&state);

        if (!(old_state & available))
        {
            {
                LOG_DEBUG << " stoping,no more append";
            }
            return;
        }

        if (old_state & (full | appending | swaping))
        {
            futex_wait(&state, old_state);
            continue;
        }

        if (!_CAS_strong_relaxed(&state, &old_state, old_state | appending)) continue;
        break;
    }

    if (cur_buffer->avail() >= len)
    {
        cur_buffer->append(logs, len);
        _store_release(&state, available);
        futex_wake(&state);
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
        _store_release(&state, full);
        futex_wake(&state);
    }
    return;
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

        for (;;)
        {
            atomic_ulong old_state = _load_acquire(&state);

            if (!(old_state & available))
            {
                {
                    LOG_DEBUG << " stoping,no more flush";
                }
                return;
            }

            if (old_state & full)
            {
                while (!_CAS_weak_relaxed(&state, &old_state, (old_state ^ full) | swaping));
                break;
            }

            if (old_state & appending)
            {
                futex_wait(&state, old_state);
                continue;
            }

            if (!_CAS_strong_relaxed(&state, &old_state, old_state | swaping)) continue;
            break;
        }

        buffers.push_back(std::move(cur_buffer));
        cur_buffer = std::move(newBuffer1);
        buffersToWrite.swap(buffers);
        if (!next_buffer)
        {
            next_buffer = std::move(newBuffer2);
        }

        _fetch_sub_release(&state, swaping);
        futex_wake(&state);

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
    _store_release(&state, 0);
    futex_wake(&state);
}