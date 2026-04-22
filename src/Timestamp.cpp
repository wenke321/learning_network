

#include "Timestamp.h"

#include <sys/time.h>

#include <cstdint>
#include <ctime>

std::string Timestamp::toStr()
{
    char buf[32] = "";

    uint64_t second      = _microSecondsSinceEpoch / 1000000;
    uint64_t microSecond = _microSecondsSinceEpoch % 1000000;

    snprintf(buf, 32, "%ld.%06ld", second, microSecond);
    return buf;
}
std::string Timestamp::toStrYMD()
{
    char buf[64];
    struct tm tm;
    time_t second = static_cast<time_t>(_microSecondsSinceEpoch / 1000000);
    gmtime_r(&second, &tm);

    snprintf(buf, 64, "%04d年%02d月%02d日%02d时%02d分%02d.%03d秒", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, second % 1000);
    return buf;
}

Timestamp Timestamp::now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t second = tv.tv_sec;
    return Timestamp(second * 1000000 + tv.tv_usec);
}

uint64_t Timestamp::now_microsecconds()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}
