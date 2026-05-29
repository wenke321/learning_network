#include "InetAddr.h"

#include <netinet/in.h>
#include <sys/socket.h>

#include <cstdint>
#include <cstring>

#include "SocketOps.h"

InetAddr::InetAddr() {}

InetAddr::InetAddr(uint16_t port, bool isLoopback, bool isIpv6)
{
    if (isIpv6)
    {
        memset(&addr6, 0, sizeof(addr6));
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port   = ::htons(port);
        addr6.sin6_addr   = isLoopback ? in6addr_loopback : in6addr_any;
    }
    else
    {
        memset(&addr4, 0, sizeof(addr4));
        addr4.sin_family      = AF_INET;
        addr4.sin_port        = ::htons(port);
        in_addr_t ip          = isLoopback ? INADDR_LOOPBACK : INADDR_ANY;
        addr4.sin_addr.s_addr = htonl(ip);
    }
}
InetAddr::InetAddr(std::string ip, uint16_t port, bool isIpv6)
{
    if (isIpv6 || strchr(ip.c_str(), ':'))
    {
        memset(&addr6, 0, sizeof(addr6));
        sockOption::fromIpPort(ip.c_str(), port, &addr6);
    }
    else
    {
        memset(&addr4, 0, sizeof(addr4));
        sockOption::fromIpPort(ip.c_str(), port, &addr4);
    }
}

const struct sockaddr* InetAddr::getSockAddr() const
{
    if (addr6.sin6_family == AF_INET) return getSockAddr4();
    return getSockAddr6();
}
const struct sockaddr* InetAddr::getSockAddr6() const { return sockOption::sockaddr_cast(&addr6); }
const struct sockaddr* InetAddr::getSockAddr4() const { return sockOption::sockaddr_cast(&addr4); }

std::string InetAddr::ipStr()
{
    if (addr6.sin6_family == AF_INET) return ipStr4();
    return ipStr6();
}

std::string InetAddr::ipStr4()
{
    char buf[64] = "";
    sockOption::toIp(buf, 64, getSockAddr4());
    return buf;
}

std::string InetAddr::ipStr6()
{
    char buf[64] = "";
    sockOption::toIp(buf, 64, getSockAddr6());
    return buf;
}

const std::string InetAddr::ipPortStr() const
{
    if (addr6.sin6_family == AF_INET) return ipPortStr4();
    return ipPortStr6();
}

const std::string InetAddr::ipPortStr4() const
{
    char buf[64] = "";
    sockOption::toIpPort(buf, 64, getSockAddr4());
    return buf;
}

const std::string InetAddr::ipPortStr6() const
{
    char buf[64] = "";
    sockOption::toIpPort(buf, 64, getSockAddr6());
    return buf;
}

void InetAddr::setScopeId(uint32_t scope_id)
{
    if (family() == AF_INET6)
    {
        addr6.sin6_scope_id = scope_id;
    }
}