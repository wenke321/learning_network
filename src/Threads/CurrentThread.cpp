#include "CurrentThread.h"

namespace CurrentThread
{
__thread int cachedTid     = 0;
__thread const char* Tname = "unknown";
__thread int tidLength     = 6;
__thread char tidString[32];

}  // namespace CurrentThread

namespace ProcessInfo
{

pid_t pid() { return ::getpid(); }

std::string hostname()
{
    // HOST_NAME_MAX 64
    // _POSIX_HOST_NAME_MAX 255
    char buf[256];
    if (::gethostname(buf, sizeof buf) == 0)
    {
        buf[sizeof(buf) - 1] = '\0';
        return buf;
    }
    else
    {
        return "unknownhost";
    }
}
};  // namespace ProcessInfo