#include <sys/types.h>
#include <unistd.h>

#include <string>
namespace CurrentThread
{

extern __thread int cachedTid;
extern __thread char tidString[32];
extern __thread int tidLength;
extern __thread const char* Tname;

void cache_Tid();

inline int tid()
{
    if (__builtin_expect(cachedTid == 0, 0))
    {
        cache_Tid();
    }
    return cachedTid;
}

inline const char* tidToString()
{
    cache_Tid();
    return tidString;
}
inline int tidLengt_() { return tidLength; }

inline const char* name() { return Tname; }

bool isSelfThread();

}  // namespace CurrentThread

namespace ProcessInfo
{
pid_t pid();

std::string hostname();
};  // namespace ProcessInfo