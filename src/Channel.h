
#pragma once
#include <functional>
#include <memory>

class EventLoop;

// when want to add a channel to loop,always use loop->add_channel(fd),
// it will create a channel and return to you,then enable read...
class Channel
{
   public:
    Channel(int fd_, EventLoop* loop_);
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
    int index_();
    void setIndex(int idx);
    bool isWriting();
    bool isReading();
    int listen_events_() const;
    int ready_events_() const;
    bool isNoneEvent() const;
    void set_ready_event(int ev);
    void tie_(const std::shared_ptr<void>&);

    void reset_listen_events();
    void set_read_callback(std::function<void()> callback);
    void set_write_callback(std::function<void()> callback);
    void set_error_callback(std::function<void()> callback);
    void set_close_callback(std::function<void()> callback);

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
    bool eventHandling;
    std::function<void()> read_callback;
    std::function<void()> write_callback;
    std::function<void()> error_callback;
    std::function<void()> close_callback;
    bool tied;
    std::weak_ptr<void> tiedObj;
};
