#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include <cstdint>
namespace sockOption
{
int createNonblockingOrDie(sa_family_t family);

int connect(int sockfd, const struct sockaddr* addr);
void bindOrDie(int sockfd, const struct sockaddr* addr);
void listenOrDie(int sockfd);
// template <typename T>
// int accept(int sockfd, T* addr);
int accept(int sockfd, struct sockaddr_in* addr);
ssize_t read(int sockfd, void* buf, size_t count);
ssize_t readv(int sockfd, const struct iovec* iov, int iovcnt);
ssize_t write(int sockfd, const void* buf, size_t count);
void close(int sockfd);
void shutdownWrite(int sockfd);

void toIpPort(char* buf, size_t size, const struct sockaddr* addr);
void toIp(char* buf, size_t size, const struct sockaddr* addr);
void fromIpPort(const char* ip, uint16_t port, struct sockaddr_in* addr);
void fromIpPort(const char* ip, uint16_t port, struct sockaddr_in6* addr);
uint16_t networkToHost16(const struct sockaddr* addr);

int getSocketError(int sockfd);

struct sockaddr* sockaddr_cast(const struct sockaddr_in* addr);
const struct sockaddr* sockaddr_cast(const struct sockaddr_in6* addr);
struct sockaddr* sockaddr_cast(struct sockaddr_in6* addr);
const struct sockaddr_in* sockaddr_in_cast(const struct sockaddr* addr);
const struct sockaddr_in6* sockaddr_in6_cast(const struct sockaddr* addr);

struct sockaddr_in6 getLocalAddr(int sockfd);
struct sockaddr_in6 getPeerAddr(int sockfd);
bool isSelfConnect(int sockfd);

/// Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).
///
void setTcpNoDelay(bool on, int sockfd);

///
/// Enable/disable SO_REUSEADDR
///
void setReuseAddr(bool on, int sockfd);

///
/// Enable/disable SO_REUSEPORT
///
void setReusePort(bool on, int sockfd);

///
/// Enable/disable SO_KEEPALIVE
///
void setKeepAlive(bool on, int sockfd);

bool getTcpInfo(struct tcp_info* tcpi, int fd);

bool getTcpInfoString(char* buf, int len, int fd);

}  // namespace sockOption