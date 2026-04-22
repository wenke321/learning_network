#pragma once

#include <cstring>

#include "LogStream.h"
#include "Timestamp.h"
class Logger
{
   public:
    enum LogLevel
    {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        NUM_LOG_LEVELS
    };

    class SourceFile
    {
       public:
        template <int N>
        constexpr SourceFile(const char (&arr)[N]) : data(arr), size(N - 1)
        {
            const char* slash = strrchr(data, '/');
            if (slash)
            {
                data = slash + 1;
                size -= static_cast<int>(data - arr);
            }
        }

        explicit SourceFile(const char* filename_) : data(filename_)
        {
            const char* slash = strrchr(data, '/');
            if (slash)
            {
                data = slash + 1;
                size = static_cast<int>(strlen(data));
            }
        }

        const char* data;
        int size;
    };

    Logger(SourceFile file, int line, LogLevel level, const char* func);
    Logger(SourceFile file, int line, LogLevel level);
    Logger(SourceFile file, int line);
    Logger(SourceFile file, int line, bool shouldAbort);
    ~Logger();

    LogStream& stream() { return impl.stream; }

    typedef void (*outputFunc)(const char* msg, int len);
    typedef void (*flushFunc)();

    static void setFlush(flushFunc);
    static void setOutput(outputFunc);
    static LogLevel logLevel();
    static void setLogLevel(LogLevel level);

   private:
    class Impl
    {
       public:
        Impl(LogLevel level_, int errno_, const SourceFile& file_, int line_);

        void formatedTime();
        void finish();

        Timestamp time;
        LogLevel level;
        SourceFile basename;
        int line;
        LogStream stream;
    };

    Impl impl;
};

extern Logger::LogLevel g_logLevel;

inline Logger::LogLevel Logger::logLevel() { return g_logLevel; }

const char* strerrorInfo(int savedErrno);

#define LOG_TRACE \
    if (Logger::logLevel() <= Logger::LogLevel::TRACE) Logger(__FILE__, __LINE__, Logger::TRACE, __func__).stream()
#define LOG_DEBUG \
    if (Logger::logLevel() <= Logger::DEBUG) Logger(__FILE__, __LINE__, Logger::DEBUG, __func__).stream()
#define LOG_INFO \
    if (Logger::logLevel() <= Logger::INFO) Logger(__FILE__, __LINE__).stream()
#define LOG_WARN     Logger(__FILE__, __LINE__, Logger::WARN).stream()
#define LOG_ERROR    Logger(__FILE__, __LINE__, Logger::ERROR).stream()
#define LOG_FATAL    Logger(__FILE__, __LINE__, Logger::FATAL).stream()
#define LOG_SYSERR   Logger(__FILE__, __LINE__, false).stream()
#define LOG_SYSFATAL Logger(__FILE__, __LINE__, true).stream()