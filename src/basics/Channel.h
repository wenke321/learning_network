
#pragma once
#include <functional>
#include <memory>

class EventLoop;

// when want to add a channel to loop,always use loop->add_channel(fd),
// it will create a channel and return to you,then enable read...
class Channel
{
   public:
    Channel(int _fd, EventLoop* _loop, bool _is_io_channel = false);
    ~Channel();

    EventLoop* owner_loop();

    void HandleEvent();
    void HandleEvent_tied();
    void EnableRead();
    void EnableWrite();
    void DisableRead();
    void DisableWrite();
    void DisableAll();
    void update();
    void remove();

    int fd_() const;
    bool tied_();
    int index_();
    void setIndex(int idx);
    void set_io_channel();
    bool isWriting();
    bool isReading();
    int listen_events_() const;
    int ready_events_() const;
    bool isNoneEvent() const;
    void set_ready_event(int ev);

    void tie_(std::shared_ptr<void>);
    void untie();

    void reset_listen_events();
    void reset_callbacks();

    void set_pri_callback(std::function<void()> callback);
    void set_in_callback(std::function<void()> callback);
    void set_out_callback(std::function<void()> callback);
    void set_err_callback(std::function<void()> callback);
    void set_hup_callback(std::function<void()> callback);
    void set_rdhup_callback(std::function<void()> callback);

    static const int READ_EVENT;
    static const int WRITE_EVENT;
    static const int NONE_EVENT;

    static const int ch_extern;
    static const int ch_added;
    static const int ch_deleted;

   private:
    int fd;
    int idx;
    EventLoop* loop;
    int listen_events;
    int ready_events;
    bool addedToLoop;
    bool is_io_channel;
    bool eventHandling;
    std::function<void()> pri_callback;
    std::function<void()> in_callback;
    std::function<void()> out_callback;
    std::function<void()> err_callback;
    std::function<void()> hup_callback;
    std::function<void()> rdhup_callback;
    bool tied;
    std::weak_ptr<void> tiedObj;
};
