
#include "Acceptor.h"

#include <fcntl.h>
#include <unistd.h>

#include <cassert>
#include <cstddef>

#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Socket.h"

Acceptor::Acceptor(EventLoop* loop, const InetAddr& listenAddr, bool reuseport) : loop_(loop), acceptSocket_(sockOption::createNonblockingOrDie(listenAddr.family())), acceptChannel_(acceptSocket_.fd_(), loop), listening_(false), idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
    assert(idleFd_ >= 0);
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bind(listenAddr);
    acceptChannel_.set_read_callback([this] { handleRead(); });
}

Acceptor::~Acceptor()
{
    acceptChannel_.DisableAll();
    acceptChannel_.remove();
    ::close(idleFd_);
}

void Acceptor::listen()
{
    LOG_TRACE << " Acceptor::listen";
    loop_->assertInLoopThread();
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.EnableRead();
}

void Acceptor::handleRead()
{
    LOG_TRACE << "Acceptor::handleRead";
    loop_->assertInLoopThread();
    InetAddr peerAddr;

    while (1)
    {
        int connfd = acceptSocket_.accept(&peerAddr);
        if (connfd >= 0)
        {
            LOG_TRACE << "new connection come";
            if (newConnectionCallback_)
            {
                LOG_TRACE << "newConnectionCallback_";
                newConnectionCallback_(connfd, peerAddr);
            }
            else
            {
                sockOption::close(connfd);
            }
        }
        else
        {
            int savedErrno = errno;
            switch (savedErrno)
            {
                case EAGAIN:
                    break;
                case ECONNABORTED:
                case EPROTO:  // ???
                case EINTR:
                case EPERM:
                    // expected errors
                    errno = savedErrno;
                case EBADF:
                case EFAULT:
                case EINVAL:
                case ENFILE:
                case EMFILE:  // per-process lmit of open file desctiptor
                case ENOBUFS:
                case ENOMEM:
                case ENOTSOCK:
                case EOPNOTSUPP:
                    // unexpected errors
                    LOG_ERROR << "unexpected error of ::accept " << savedErrno;
                    break;
                default:
                    LOG_ERROR << "unknown error of ::accept " << savedErrno;
                    break;
            }

            // LOG_SYSERR << "in Acceptor::handleRead";
            // // Read the section named "The special problem of
            // // accept()ing when you can't" in libev's doc.
            // // By Marc Lehmann, author of libev.
            // if (errno == EMFILE)
            // {
            //     ::close(idleFd_);
            //     idleFd_ = ::accept(acceptSocket_.fd_(), NULL, NULL);
            //     ::close(idleFd_);
            //     idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
            // }
        }
    }
}
