
#include "Socket.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <cstring>

#include "InetAddr.h"
#include "Logger.h"
#include "SocketOps.h"

Socket::Socket(int fd_) : fd(fd_) {}

Socket::~Socket() { sockOption::close(fd); }

std::string Socket::get_addr() const
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    socklen_t len = sizeof(addr);
    if (getpeername(fd, (struct sockaddr*)&addr, &len) == -1)
    {
        return "";
    }
    std::string ret(inet_ntoa(addr.sin_addr));
    ret += ":";
    ret += std::to_string(htons(addr.sin_port));
    return ret;
}

void Socket::bind(const InetAddr localAddr)
{
    if (localAddr.family() == AF_INET)
        bind4(localAddr);
    else
        bind6(localAddr);
}

void Socket::bind4(const InetAddr localAddr) const { sockOption::bindOrDie(fd, localAddr.getSockAddr4()); }
void Socket::bind6(const InetAddr localAddr) const { sockOption::bindOrDie(fd, localAddr.getSockAddr6()); }

void Socket::listen() const { sockOption::listenOrDie(fd); }

int Socket::accept(InetAddr* peerAddr) const
{
    LOG_TRACE << "Socket::accept";
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    int connfd = sockOption::accept(fd, &addr);
    if (connfd >= 0)
    {
        peerAddr->setSockAddr4(addr);
    }
    return connfd;
}

void Socket::shutdownWrite() { sockOption::shutdownWrite(fd); }
void Socket::shutdown() { sockOption::shutdown(fd); }

void Socket::close() { sockOption::close(fd); }

void Socket::setTcpNoDelay(bool on) { sockOption::setTcpNoDelay(on, fd); }
void Socket::setReuseAddr(bool on) { sockOption::setReuseAddr(on, fd); }
void Socket::setReusePort(bool on) { sockOption::setReusePort(on, fd); }
void Socket::setKeepAlive(bool on) { sockOption::setKeepAlive(on, fd); }

bool Socket::getTcpInfo(struct tcp_info* tcpi) { return sockOption::getTcpInfo(tcpi, fd); }
bool Socket::getTcpInfoString(char* buf, int len) { return sockOption::getTcpInfoString(buf, len, fd); }
