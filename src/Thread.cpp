#include "Thread.h"

#include <linux/prctl.h>
#include <pthread.h>
#include <sched.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>

#include "Logger.h"

pid_t gettid() { return static_cast<pid_t>(::syscall(SYS_gettid)); }

void CurrentThread::cache_Tid()
{
    if (cachedTid == 0)
    {
        cachedTid = ::gettid();
        tidLength = snprintf(tidString, sizeof(tidString), "%5d", cachedTid);
    }
}

bool CurrentThread::isSelfThread() { return tid() == gettid(); }

struct ThreadData
{
    ThreadFunc& func_;
    CountDownLatch* latch_;
    pid_t* pid_;
    const std::string& name_;

    ThreadData(ThreadFunc& func_, CountDownLatch* _latch_, pid_t* _pid_, const std::string& _name_) : func_(func_), latch_(_latch_), pid_(_pid_), name_(_name_) {}
    void runInThread()
    {
        *pid_ = CurrentThread::tid();
        pid_  = nullptr;
        latch_->countDown();
        latch_ = nullptr;

        CurrentThread::Tname = name_.empty() ? "kwThread" : name_.c_str();
        ::prctl(PR_SET_NAME, CurrentThread::name());

        try
        {
            func_();
            CurrentThread::Tname = "finished";
        }
        catch (const std::exception& ex)
        {
            CurrentThread::Tname = "crashed";
            int err              = errno;
            {
                LOG_SYSFATAL << "exception caught in Thread : " << name_.c_str() << "\n" << "reason : " << ex.what() << "\n" << "errno : " << err;
            }
        }
        catch (...)
        {
            CurrentThread::Tname = "crashed";
            fprintf(stderr, "unknown exception caught in Thread %s\n", name_.c_str());
            throw;  // rethrow
        }
    }
};

void* startThread(void* data_)
{
    ThreadData* data = static_cast<ThreadData*>(data_);
    data->runInThread();
    delete data;
    return nullptr;
}

Thread::Thread(ThreadFunc func, const std::string& name) : started(0), joined_(0), pthread_id(0), pid(0), name(name), latch(1), threadFunc(func) { setDefaultName(); }

void Thread::setDefaultName()
{
    uint32_t num = __atomic_add_fetch(&ThreadNum, 1, __ATOMIC_SEQ_CST);
    if (name.empty())
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "Thread%d", num);
        name = buf;
    }
}

void Thread::start()
{
    assert(!started);
    started = true;

    ThreadData* data = new ThreadData(threadFunc, &latch, &pid, name);
    if (::pthread_create(&pthread_id, 0, &startThread, data))
    {
        started = false;
        delete data;
        // log err
    }
    else
    {
        latch.wait();
        assert(pid > 0);
    }
}

int Thread::join()
{
    assert(started);
    assert(!joined_);
    joined_ = true;
    return ::pthread_join(pthread_id, NULL);
}