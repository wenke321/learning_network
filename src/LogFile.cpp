#include "LogFile.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>

#include "FileUtil.h"

LogFile::LogFile(const std::string& basename_, unsigned int rollSize_, int flushInterval_, int checkInterval_, bool threadSafe) : count(0), basename(basename_), rollSize(rollSize_), flushInterval(flushInterval_), checkInterval(checkInterval_), mutex(threadSafe ? std::make_unique<MutexLock>() : nullptr), startOfPeriod(0), lastRoll(0), lastFlush(0)
{
    assert(basename.find('/') == std::string::npos);
    roll();
}

void LogFile::append(const char* logs, int len)
{
    if (mutex)
    {
        MutexLockGuard lock(*mutex);
        append_impl(logs, len);
    }
    else
        append_impl(logs, len);
}

// void LogFile::append(const iovec* iov, int iovcnt)
// {
//     if (mutex)
//     {
//         MutexLockGuard lock(*mutex);
//         file->append(iov, iovcnt);
//     }
//     else
//         file->append(iov, iovcnt);
// }

void LogFile::flush() { file->flush(); }

void LogFile::append_impl(const char* logs, int len)
{
    file->append(logs, len);
    if (file->writtenBytes_() >= rollSize)
        roll();
    else
    {
        count++;
        if (count == checkInterval)
        {
            count = 0;

            time_t now;
            ::time(&now);
            time_t now_period = now / RollPerSeconds;
            if (now_period >= startOfPeriod)
            {
                roll();
            }

            if (now - lastFlush >= flushInterval)
            {
                lastFlush = now;
                file->flush();
            }
        }
    }
}

bool LogFile::roll()
{
    time_t now          = 0;
    std::string newname = getLogFileName(basename, now);
    time_t newstart     = now / RollPerSeconds * RollPerSeconds;

    if (now > startOfPeriod)
    {
        lastRoll      = now;
        lastFlush     = now;
        startOfPeriod = newstart;
        file.reset(new AppendFile(newname));
        return true;
    }

    return false;
}

std::string LogFile::getLogFileName(const std::string& basename_, time_t& now_)
{
    std::string filename;
    filename.reserve(basename_.size() + 64);
    filename = basename_;

    char timebuf[32];
    struct tm tm;
    now_ = time(NULL);
    gmtime_r(&now_, &tm);
    strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm);
    filename += timebuf;

    filename += ProcessInfo::hostname();

    char pidbuf[32];
    snprintf(pidbuf, sizeof pidbuf, ".%d", ProcessInfo::pid());
    filename += pidbuf;

    filename += ".log";

    return filename;
}