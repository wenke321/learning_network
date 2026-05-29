#include "LogStream.h"

size_t convertHex(char buf[], uintptr_t value)
{
    uintptr_t i = value;
    char* p     = buf;

    do
    {
        int lsd = static_cast<int>(i % 16);
        i /= 16;
        *p++ = digitsHex[lsd];
    } while (i != 0);

    *p = '\0';
    std::reverse(buf, p);

    return p - buf;
}
