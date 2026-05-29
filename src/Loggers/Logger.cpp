#include "Logger.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "../Threads/CurrentThread.h"
#include "../Timers/Timestamp.h"
#include "LogStream.h"

__thread char t_errnobuf[512];
__thread char t_time[64];
__thread time_t t_lastSecond;

const char* strerrorInfo(int savedErrno) { return strerror_r(savedErrno, t_errnobuf, sizeof(t_errnobuf)); }

const char* LogLevelName[Logger::NUM_LOG_LEVELS] = {
    "TRACE ", "DEBUG ", "INFO  ", "WARN  ", "ERROR ", "FATAL ",
};

Logger::LogLevel initLogLevel()
{
    if (::getenv("LOG_TRACE"))
        return Logger::TRACE;
    else if (::getenv("LOG_DEBUG"))
        return Logger::DEBUG;
    else
        return Logger::INFO;
}

Logger::LogLevel g_logLevel = initLogLevel();

class T
{
   public:
    constexpr T(const char* str_, unsigned int len_) : str(str_), len(len_) {}

    const char* str;
    const unsigned len;
};

inline LogStream& operator<<(LogStream& s, T str_)
{
    s.append(str_.str, str_.len);
    return s;
}

inline LogStream& operator<<(LogStream& s, const Logger::SourceFile& file)
{
    s.append(file.data, file.size);
    return s;
}

void defaultOutput(const char* msg, int len) { size_t n = ::fwrite(msg, 1, len, stdout); }

void defaultFlush() { fflush(stdout); }

Logger::outputFunc g_output = defaultOutput;
Logger::flushFunc g_flush   = defaultFlush;

std::string microseconds_to_utc_string(int64_t microseconds_since_epoch)
{
    // 1. 分离秒和微秒部分
    time_t seconds       = static_cast<time_t>(microseconds_since_epoch / 1000000);
    int64_t us_remainder = microseconds_since_epoch % 1000000;
    // 确保微秒部分为非负数（当输入为负数时需处理）
    if (us_remainder < 0)
    {
        // 理论上微秒部分应在 [0, 999999] 区间，但为安全处理负数输入：
        seconds -= 1;
        us_remainder += 1000000;
    }

    // 2. 将秒数转换为 UTC 日历字段（线程安全版本）
    std::tm tm_buf;
    gmtime_r(&seconds, &tm_buf);

    // 3. 格式化输出
    char buffer[32];  // 足够容纳 "YYYY-MM-DD HH:MM:SS.uuuuuu" + 终止符
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%06lld", tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday, tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, static_cast<long long>(us_remainder));

    return std::string(buffer);
}

std::string microseconds_to_localtime_string(int64_t microseconds_since_epoch)
{
    // 1. 分离秒和微秒部分
    time_t seconds       = static_cast<time_t>(microseconds_since_epoch / 1000000);
    int64_t us_remainder = microseconds_since_epoch % 1000000;
    // 处理负数微秒余数
    if (us_remainder < 0)
    {
        seconds -= 1;
        us_remainder += 1000000;
    }

    // 2. 将秒数转换为本地日历字段（线程安全版本）
    std::tm tm_buf;
    localtime_r(&seconds, &tm_buf);

    // 3. 格式化输出
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%06lld", tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday, tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, static_cast<long long>(us_remainder));

    return std::string(buffer);
}

Logger::Impl::Impl(LogLevel level_, int errno_, const SourceFile& file_, int line_) : time(Timestamp::now_microsecconds()), level(level_), basename(file_), line(line_), stream()
{
    formatedTime();
    stream << "tid:" << T(CurrentThread::tidToString(), CurrentThread::tidLength) << " ";
    stream << LogLevelName[level];
    if (errno_)
    {
        stream << strerrorInfo(errno_) << " (errno=" << errno_ << ") ";
    }
}

void Logger::Impl::formatedTime()
{
    // 1. 分离秒和微秒部分
    time_t seconds       = static_cast<time_t>(time.microSecondsSinceEpoch() / 1000000);
    int64_t us_remainder = time.microSecondsSinceEpoch() % 1000000;

    // 确保微秒部分为非负数（当输入为负数时需处理）
    if (us_remainder < 0)
    {
        // 理论上微秒部分应在 [0, 999999] 区间，但为安全处理负数输入：
        seconds -= 1;
        us_remainder += 1000000;
    }

    // 2. 将秒数转换为 UTC 日历字段（线程安全版本）
    std::tm tm_buf;
    gmtime_r(&seconds, &tm_buf);

    // 3. 格式化输出
    char buffer[32];  // 足够容纳 "YYYY-MM-DD HH:MM:SS.uuuuuu" + 终止符
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%06lld", tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday, tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, static_cast<long long>(us_remainder));
    stream << "[" << T(buffer, 26) << "] ";
}

void Logger::Impl::finish() { stream << " - " << basename << ':' << line << '\n'; }

Logger::Logger(SourceFile file_, int line_, LogLevel level_, const char* func_) : impl(level_, 0, file_, line_) { impl.stream << func_ << ""; }
Logger::Logger(SourceFile file, int line, LogLevel level) : impl(level, 0, file, line) {}
Logger::Logger(SourceFile file, int line) : impl(Logger::INFO, 0, file, line) {}
Logger::Logger(SourceFile file, int line, bool shouldAbort) : impl(shouldAbort ? Logger::ERROR : Logger::INFO, errno, file, line) {}

Logger::~Logger()
{
    impl.finish();
    const LogStream::Buffer& buf(impl.stream.buffer_());
    g_output(buf.data_(), buf.length());
    g_flush();
    if (g_logLevel == Logger::FATAL)
    {
        abort();
    }
}

void Logger::setLogLevel(Logger::LogLevel level_) { g_logLevel = level_; }

void Logger::setOutput(Logger::outputFunc outputFunc_) { g_output = outputFunc_; }

void Logger::setFlush(Logger::flushFunc flushFunc_) { g_flush = flushFunc_; }