#pragma once

#include <netinet/in.h>

#include <cstdint>
#include <cstring>
#include <string>

class InetAddr
{
   public:
    explicit InetAddr(uint16_t port, bool isLoopback, bool isIpv6);
    explicit InetAddr(std::string ip, uint16_t port, bool isIpv6);
    InetAddr();
    explicit InetAddr(const struct sockaddr_in addr) : addr4(addr) {}
    explicit InetAddr(const struct sockaddr_in6 addr) : addr6(addr) {}

    const struct sockaddr* getSockAddr() const;
    const struct sockaddr* getSockAddr6() const;
    const struct sockaddr* getSockAddr4() const;
    void setSockAddr4(const struct sockaddr_in& addr4_) { addr4 = addr4_; }
    void setSockAddr6(const struct sockaddr_in6& addr6_) { addr6 = addr6_; }
    sa_family_t family() const { return addr4.sin_family; }
    std::string ipStr();
    std::string ipStr4();
    std::string ipStr6();
    const std::string ipPortStr() const;
    const std::string ipPortStr4() const;
    const std::string ipPortStr6() const;
    void setScopeId(uint32_t);

   private:
    union
    {
        struct sockaddr_in addr4;
        struct sockaddr_in6 addr6;
    };
};