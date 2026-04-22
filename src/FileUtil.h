#pragma once

#include <fcntl.h>

#include <cstddef>
#include <cstdio>
#include <string>
class AppendFile
{
   public:
    AppendFile(const std::string&);
    ~AppendFile();

    void append(const char* logs, size_t len);
    // void append(const iovec* iov, int iovcnt);
    void flush();
    unsigned int writtenBytes_() { return writtenBytes; }

   private:
    size_t write(const char* logs, size_t len);
    FILE* fd;
    char buffer[64 * 1024];
    off_t writtenBytes;
};