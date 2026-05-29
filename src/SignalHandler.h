#pragma once

#include <bits/types/sigset_t.h>
#include <sys/signalfd.h>

#include <functional>

#include "basics/Channel.h"

// template <typename Sig, typename... Args>
// void add_signal(Sig& mask, Args... args)
// {
//     (sigaddset(&mask, args), ...);
// }

class SignalHandler
{
   public:
    using handle_func = std::function<void()>;
    SignalHandler();
    ~SignalHandler();

    void init(EventLoop* _loop);
    int get_fd();

    Channel* get_ch();

    void set_handle_int(handle_func);
    void set_handle_hup(handle_func);
    void set_handle_term(handle_func);
    void set_handle_quit(handle_func);
    void set_handle_abort(handle_func);
    void set_handle_child(handle_func);

    static void default_handle_int();
    static void default_handle_hup();
    static void default_handle_term();
    static void default_handle_quit();
    static void default_handle_abort();
    static void default_handle_child();

   private:
    void default_handler();

    sigset_t mask;

    handle_func handle_sig_int;
    handle_func handle_sig_hup;
    handle_func handle_sig_term;
    handle_func handle_sig_quit;
    handle_func handle_sig_segv;
    handle_func handle_sig_abort;
    handle_func handle_sig_child;

    Channel* signal_channel;
    bool sig_cared[65];
};