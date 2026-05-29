
#pragma once
#include <netinet/tcp.h>

#include <string>

#include "InetAddr.h"

class Socket
{
   public:
    explicit Socket(int fd_);
    ~Socket();
    void set_fd(int fd);

    std::string get_addr() const;
    int fd_() { return fd; }
    void bind(const InetAddr localAddr);
    void bind4(const InetAddr localAddr) const;
    void bind6(const InetAddr localAddr) const;
    void listen() const;
    int accept(InetAddr* peeraddr) const;
    void shutdownWrite();
    void shutdown();
    void close();
    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);
    bool getTcpInfo(struct tcp_info* tcpi);
    bool getTcpInfoString(char* buf, int len);

   private:
    int fd;
};
