#pragma once

#include <fcntl.h>
#include <sys/uio.h>

#include <ctime>
#include <memory>
#include <string>

#include "CountDownLatch.h"
#include "FileUtil.h"
class LogFile
{
   public:
    LogFile(const std::string& basename_, unsigned int rollSize_, int flushInterval_, int checkInterval_, bool threadSafe);

    void append(const char* logs, int len);
    // void append(const iovec* iov, int iovcnt);
    bool roll();
    void flush();

   private:
    void append_impl(const char* logs, int len);

    static std::string getLogFileName(const std::string& basename_, time_t& now);

    int count;

    const std::string basename;
    const unsigned int rollSize;
    const int flushInterval;
    const int checkInterval;

    const std::unique_ptr<MutexLock> mutex;
    time_t startOfPeriod;
    time_t lastRoll;
    time_t lastFlush;

    std::unique_ptr<AppendFile> file;

    const static int RollPerSeconds = 60 * 60 * 24;
};