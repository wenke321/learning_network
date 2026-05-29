#include "FileUtil.h"

#include <sys/uio.h>

#include <cassert>
#include <cstddef>
#include <cstdio>

#include "Logger.h"

AppendFile::AppendFile(const std::string& _filename) : writtenBytes(0)
{
    fd = ::fopen(_filename.c_str(), "ae");
    assert(fd);
    ::setbuffer(fd, buffer, sizeof(buffer));
}

AppendFile::~AppendFile() { ::fclose(fd); }

void AppendFile::append(const char* logs, size_t len)
{
    size_t written = 0;
    while (written < len)
    {
        size_t remain = len - written;
        size_t n      = write(logs, len);

        if (n != remain)
        {
            int err = ferror_unlocked(fd);
            if (err)
            {
                fprintf(stderr, "AppendFile::append() failed %s\n", strerrorInfo(err));
                break;
            }
        }
        written += n;
    }
    writtenBytes += written;
}

// void AppendFile::append(const iovec* iov, int iovcnt) {
//     while(iovcnt>0){
//         size_t n=writev(fd->,)
//     }
// }

void AppendFile::flush() { ::fflush(fd); }

size_t AppendFile::write(const char* logs, size_t len) { return ::fwrite_unlocked(logs, 1, len, fd); }