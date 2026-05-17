#include "SignalHandler.h"

#include <sys/signalfd.h>

#include <csignal>

#include "EventLoop.h"
#include "Logger.h"

int create_sigfd(sigset_t& mask)
{
    int fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (fd < 0)
    {
        LOG_ERROR << "create_sigfd failed";
    }
    return fd;
}

SignalHandler::SignalHandler() {}

SignalHandler::~SignalHandler()
{
    LOG_TRACE << " ";
    signal_channel->DisableAll();
    signal_channel->remove();
}

void SignalHandler::default_handler()
{
    struct signalfd_siginfo sig_info;

    if (read(signal_channel->fd_(), &sig_info, sizeof(signalfd_siginfo)) == sizeof(signalfd_siginfo))
    {
        LOG_INFO << "receive signal=" << sig_info.ssi_signo;
    }
    else
    {
        LOG_ERROR << "";
    }

    switch (sig_info.ssi_signo)
    {
        case SIGINT:
            handle_sig_int();
            break;

        case SIGQUIT:
            handle_sig_quit();
            break;

        case SIGHUP:
            handle_sig_hup();
            break;

        case SIGTERM:
            handle_sig_term();
            break;

        case SIGCHLD:
            handle_sig_child();
            break;

        case SIGABRT:
            handle_sig_abort();
            break;
    }
}

void SignalHandler::set_handle_int(handle_func _handle_int = default_handle_int)
{
    handle_sig_int    = _handle_int;
    sig_cared[SIGINT] = true;
}
void SignalHandler::set_handle_hup(handle_func _handle_hup = default_handle_hup)
{
    handle_sig_hup    = _handle_hup;
    sig_cared[SIGHUP] = true;
}
void SignalHandler::set_handle_term(handle_func _handle_term = default_handle_term)
{
    handle_sig_term    = _handle_term;
    sig_cared[SIGTERM] = true;
}
void SignalHandler::set_handle_quit(handle_func _handle_quit = default_handle_quit)
{
    handle_sig_quit    = _handle_quit;
    sig_cared[SIGQUIT] = true;
}
void SignalHandler::set_handle_abort(handle_func _handle_abort = default_handle_abort)
{
    handle_sig_abort   = _handle_abort;
    sig_cared[SIGABRT] = true;
}
void SignalHandler::set_handle_child(handle_func _handle_child = default_handle_child)
{
    handle_sig_child   = _handle_child;
    sig_cared[SIGCHLD] = true;
}

void SignalHandler::default_handle_int() { LOG_TRACE << " default_handle_int"; }

void SignalHandler::default_handle_hup() { LOG_TRACE << " default_handle_hup"; }

void SignalHandler::default_handle_term() { LOG_TRACE << " default_handle_term"; }
void SignalHandler::default_handle_quit() { LOG_TRACE << " default_handle_quit"; }

void SignalHandler::default_handle_abort() { LOG_TRACE << " default_handle_abort"; }

void SignalHandler::default_handle_child() { LOG_TRACE << " default_handle_child"; }

void SignalHandler::init(EventLoop* _loop)
{
    LOG_DEBUG << " ";
    sigemptyset(&mask);
    for (int i = 0; i < 65; i++)
    {
        if (sig_cared[i]) sigaddset(&mask, i);
    }

    int fd = create_sigfd(mask);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);
    signal_channel = _loop->add_channel(fd);
    signal_channel->set_in_callback([&] { default_handler(); });
    signal_channel->EnableRead();
}

int SignalHandler::get_fd() { return signal_channel->fd_(); }

Channel* SignalHandler::get_ch() { return signal_channel; }
