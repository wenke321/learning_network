#include "SocketOps.h"

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include "Logger.h"

struct sockaddr* sockOption::sockaddr_cast(const struct sockaddr_in* addr) { return static_cast<struct sockaddr*>((void*)addr); }
const struct sockaddr* sockOption::sockaddr_cast(const struct sockaddr_in6* addr) { return static_cast<const struct sockaddr*>((const void*)addr); }
struct sockaddr* sockOption::sockaddr_cast(struct sockaddr_in6* addr) { return static_cast<struct sockaddr*>((void*)addr); }
const struct sockaddr_in* sockOption::sockaddr_in_cast(const struct sockaddr* addr) { return static_cast<const struct sockaddr_in*>((const void*)addr); }
const struct sockaddr_in6* sockOption::sockaddr_in6_cast(const struct sockaddr* addr) { return static_cast<const struct sockaddr_in6*>((const void*)addr); }

int sockOption::createNonblockingOrDie(sa_family_t family)
{
    int sockfd = socket(family, SOCK_NONBLOCK | SOCK_CLOEXEC | SOCK_STREAM, IPPROTO_TCP);

    if (sockfd < 0)
    {
        LOG_ERROR << "create sockfd fail";
    }

    return sockfd;
}

int sockOption::connect(int sockfd, const struct sockaddr* addr) { return ::connect(sockfd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in))); }
void sockOption::bindOrDie(int sockfd, const struct sockaddr* addr)
{
    LOG_TRACE << "sockOption::bindOrDie";
    int ret = bind(sockfd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
    if (ret < 0)
    {
        LOG_ERROR << "bind error,errno=" << errno << " fd=" << sockfd << " addr=" << inet_ntoa(reinterpret_cast<const sockaddr_in*>(addr)->sin_addr);
    }
}
void sockOption::listenOrDie(int sockfd)
{
    int ret = ::listen(sockfd, SOMAXCONN);
    if (ret < 0)
    {
        LOG_ERROR << "listen error,fd=" << sockfd;
    }
}

int sockOption::accept(int sockfd, struct sockaddr_in* addr)
{
    LOG_TRACE << "Socket::accept";
    socklen_t addrlen = static_cast<socklen_t>(sizeof(sockaddr_in));
    return ::accept4(sockfd, sockaddr_cast(addr), &addrlen, SOCK_CLOEXEC | SOCK_NONBLOCK);

    // if (connfd < 0)
    // {
    //     int savedErrno = errno;
    //     switch (savedErrno)
    //     {
    //         case EAGAIN:
    //         case ECONNABORTED:
    //         case EPROTO:  // ???
    //         case EINTR:
    //         case EPERM:
    //         case EMFILE:  // per-process lmit of open file desctiptor ???
    //             // expected errors
    //             errno = savedErrno;
    //             break;
    //         case EBADF:
    //         case EFAULT:
    //         case EINVAL:
    //         case ENFILE:
    //         case ENOBUFS:
    //         case ENOMEM:
    //         case ENOTSOCK:
    //         case EOPNOTSUPP:
    //             // unexpected errors
    //             LOG_ERROR << "unexpected error of ::accept " << savedErrno;
    //             break;
    //         default:
    //             LOG_ERROR << "unknown error of ::accept " << savedErrno;
    //             break;
    //     }
    // }
    // return connfd;
}
ssize_t sockOption::read(int sockfd, void* buf, size_t count) { return ::read(sockfd, buf, count); }
ssize_t sockOption::readv(int sockfd, const struct iovec* iov, int iovcnt) { return readv(sockfd, iov, iovcnt); }
ssize_t sockOption::write(int sockfd, const void* buf, size_t count) { return ::write(sockfd, buf, count); }
void sockOption::close(int sockfd)
{
    if (::close(sockfd) < 0)
    {
        LOG_TRACE << "socket close,fd=" << sockfd;
    }
}

void sockOption::shutdownWrite(int sockfd)
{
    if (::shutdown(sockfd, SHUT_WR) < 0)
    {
        LOG_TRACE << "shutdown,fd=" << sockfd;
    }
}

void sockOption::toIpPort(char* buf, size_t size, const struct sockaddr* addr)
{
    if (addr->sa_family == AF_INET6)
    {
        buf[0] = '[';
        toIp(buf + 1, size - 1, addr);
        size_t end                       = ::strlen(buf);
        const struct sockaddr_in6* addr6 = sockOption::sockaddr_in6_cast(addr);
        uint16_t port                    = ::ntohs(addr6->sin6_port);
        assert(size > (end + 4));
        snprintf(buf + end, size - end, "]:%u", port);
    }
    toIp(buf, size, addr);
    size_t end                      = ::strlen(buf);
    const struct sockaddr_in* addr4 = sockaddr_in_cast(addr);
    uint16_t port                   = ::ntohs(addr4->sin_port);
    assert(size > end);
    snprintf(buf + end, size - end, ":%u", port);
}
void sockOption::toIp(char* buf, size_t size, const struct sockaddr* addr)
{
    if (addr->sa_family == AF_INET)
    {
        assert(size >= INET_ADDRSTRLEN);
        const struct sockaddr_in* addr4 = sockaddr_in_cast(addr);
        ::inet_ntop(AF_INET, &addr4->sin_addr, buf, size);
    }
    else if (addr->sa_family == AF_INET6)
    {
        assert(size >= INET_ADDRSTRLEN);
        const struct sockaddr_in6* addr6 = sockaddr_in6_cast(addr);
        ::inet_ntop(AF_INET6, &addr6->sin6_addr, buf, size);
    }
}

void sockOption::fromIpPort(const char* ip, uint16_t port, struct sockaddr_in* addr)
{
    addr->sin_family = AF_INET;
    addr->sin_port   = htons(port);
    if (::inet_pton(AF_INET, ip, &addr->sin_addr) <= 0)
    {
        // log << "sockets::fromIpPort";
    }
}
void sockOption::fromIpPort(const char* ip, uint16_t port, struct sockaddr_in6* addr)
{
    addr->sin6_family = AF_INET;
    addr->sin6_port   = htons(port);
    if (::inet_pton(AF_INET, ip, &addr->sin6_addr) <= 0)
    {
        // log << "sockets::fromIpPort";
    }
}

struct sockaddr_in6 sockOption::getLocalAddr(int sockfd)
{
    struct sockaddr_in6 addr6;
    memset(&addr6, 0, sizeof(addr6));
    socklen_t len = static_cast<socklen_t>(sizeof(addr6));
    if (::getsockname(sockfd, sockOption::sockaddr_cast(&addr6), &len) < 0)
    {
        LOG_ERROR << "::getsockname error";
    }
    return addr6;
}
struct sockaddr_in6 sockOption::getPeerAddr(int sockfd)
{
    struct sockaddr_in6 addr6;
    memset(&addr6, 0, sizeof(addr6));
    socklen_t len = static_cast<socklen_t>(sizeof(addr6));
    if (::getpeername(sockfd, sockOption::sockaddr_cast(&addr6), &len) < 0)
    {
        LOG_ERROR << "::getpeername error";
    }
    return addr6;
}

bool sockOption::isSelfConnect(int sockfd)
{
    struct sockaddr_in6 localaddr = getLocalAddr(sockfd);
    struct sockaddr_in6 peeraddr  = getPeerAddr(sockfd);
    if (localaddr.sin6_family == AF_INET)
    {
        const struct sockaddr_in* laddr4 = reinterpret_cast<struct sockaddr_in*>(&localaddr);
        const struct sockaddr_in* raddr4 = reinterpret_cast<struct sockaddr_in*>(&peeraddr);
        return laddr4->sin_port == raddr4->sin_port && laddr4->sin_addr.s_addr == raddr4->sin_addr.s_addr;
    }
    else if (localaddr.sin6_family == AF_INET6)
    {
        return localaddr.sin6_port == peeraddr.sin6_port && memcmp(&localaddr.sin6_addr, &peeraddr.sin6_addr, sizeof(localaddr.sin6_addr)) == 0;
    }
    else
    {
        return false;
    }
}

int sockOption::getSocketError(int sockfd)
{
    int op;
    socklen_t l = static_cast<socklen_t>(sizeof(l));
    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &op, &l) < 0) return errno;
    return op;
}

void sockOption::setTcpNoDelay(bool on, int sockfd)
{
    int opVal = on ? 1 : 0;
    if (::setsockopt(sockfd, SOL_SOCKET, TCP_NODELAY, &opVal, static_cast<socklen_t>(opVal)) < 0)
    {
        LOG_ERROR << "::setsockopt(TCP_NODELAY),errno=" << errno;
    }
}

void sockOption::setReuseAddr(bool on, int sockfd)
{
    LOG_TRACE << "::setsockopt(SO_REUSEADDR)";
    int opVal = on ? 1 : 0;
    if (::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opVal, sizeof(opVal)) < 0)
    {
        LOG_ERROR << "::setsockopt(SO_REUSEADDR),errno=" << errno;
    }
}
void sockOption::setReusePort(bool on, int sockfd)
{
    LOG_TRACE << "::setsockopt(SO_REUSEPORT),errno=";
    int opVal = on ? 1 : 0;
    if (::setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opVal, sizeof(opVal)) < 0)
    {
        LOG_ERROR << "::setsockopt(SO_REUSEPORT),errno=" << errno;
    }
}
void sockOption::setKeepAlive(bool on, int sockfd)
{
    int opVal = on ? 1 : 0;
    if (::setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &opVal, sizeof(opVal)) < 0)
    {
        LOG_ERROR << "::setsockopt(SO_KEEPALIVE),errno=" << errno;
    }
}

bool sockOption::getTcpInfo(struct tcp_info* tcpi, int fd)
{
    socklen_t len = sizeof(*tcpi);
    memset(tcpi, 0, len);
    return ::getsockopt(fd, SOL_TCP, TCP_INFO, tcpi, &len) == 0;
}

bool sockOption::getTcpInfoString(char* buf, int len, int fd)
{
    struct tcp_info tcpi;
    bool ok = getTcpInfo(&tcpi, fd);
    if (ok)
    {
        snprintf(buf, len,
                 "unrecovered=%u "
                 "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
                 "lost=%u retrans=%u rtt=%u rttvar=%u "
                 "sshthresh=%u cwnd=%u total_retrans=%u",
                 tcpi.tcpi_retransmits,  // Number of unrecovered [RTO] timeouts
                 tcpi.tcpi_rto,          // Retransmit timeout in usec
                 tcpi.tcpi_ato,          // Predicted tick of soft clock in usec
                 tcpi.tcpi_snd_mss, tcpi.tcpi_rcv_mss,
                 tcpi.tcpi_lost,     // Lost packets
                 tcpi.tcpi_retrans,  // Retransmitted packets out
                 tcpi.tcpi_rtt,      // Smoothed round trip time in usec
                 tcpi.tcpi_rttvar,   // Medium deviation
                 tcpi.tcpi_snd_ssthresh, tcpi.tcpi_snd_cwnd,
                 tcpi.tcpi_total_retrans);  // Total retransmits for entire connection
    }
    return ok;
}