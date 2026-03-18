#include "corio/kernel_events.h"
#include "corio/error.h"
#include <unistd.h>
#include <assert.h>

namespace corio 
{
    enum event_fd_flags {
        EVENT_FD_UNIQUE = 1 << 20
    };

    event_fd::event_fd(unique, int fd_ = -1) :
        fd(fd_),
        flags(EVENT_FD_UNIQUE) { }

    event_fd::event_fd(shared, int fd_ = -1) :
        fd(fd_),
        flags(0) { }

    event_fd::~event_fd() {
        if(flags & EVENT_FD_UNIQUE) 
            this->close();
    }

    int event_fd::operator*() {
        return fd;
    }

    event_fd::operator bool() {
        return fd != -1;
    }

    void event_fd::assign(const int fd_) {
        assert(fd == -1);
        fd = fd_;
    }

    void event_fd::close() {
        if(*this) ::close(fd);
    }

    kernel_events::kernel_events(const int max) :
        num_events(0),
        max_events(max),
        position(0),
        efd(-1),
        events(new epoll_event[max])
    {
        assert(max_events >= 0);
        efd = ::epoll_create(max_events); 
        if(efd == -1) 
            throw system_error("::epoll_create");
    }

    kernel_events::~kernel_events() {
        close();
    }

    void kernel_events::close() {
        if(efd == -1) 
            return;
        ::close(efd);
        efd = -1;
    }

    void kernel_events::control(
        event_fd &fd,
        const int op, 
        const int flags, 
        event ev)
    {
        assert(fd);
        struct epoll_event ep_ev = {
            .events = flags | ev.events,
            .data = { .u64 = ev.data.u64 } };
        if(::epoll_ctl(efd, op, *fd, &ep_ev) == -1)
            throw system_error("::epoll_ctl");
    }

    int kernel_events::wait(const int timeout) {
        num_events = 0;
        position = 0;
        int n = -1;
        if((n = ::epoll_wait(efd, events.get(), max_events, timeout)) == -1)
            throw system_error("::epoll_wait");
        num_events = n;
        return n;
    }

    event kernel_events::iterator::operator*() {
        return {
            .events = pointer->events,
            .data = { .u64 = pointer->data.u64 }};
    }

    kernel_events::iterator &kernel_events::iterator::operator++() {
        ++pointer;
        return *this;
    }

    bool kernel_events::iterator::operator!=(iterator &other) {
        return pointer != other.pointer;
    }

    kernel_events::iterator kernel_events::begin() {
        return iterator{ events.get() };
    }

    kernel_events::iterator kernel_events::end() {
        return iterator{ events.get() + num_events };
    }
    
    int kernel_events::get_num_events() {
        return num_events;
    }

    int kernel_events::get_max_events() {
        return max_events;
    }
}
