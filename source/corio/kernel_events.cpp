#include "corio/kernel_events.h"
#include "corio/error.h"
#include <unistd.h>
#include <assert.h>

namespace corio 
{
    kernel_events::kernel_events(const int max) :
        num_events(0),
        max_events(max),
        position(0),
        fd(-1),
        events(new epoll_event[max])
    {
        assert(max_events >= 0);
        fd = ::epoll_create(max_events); 
        if(fd == -1) 
            throw system_error("::epoll_create");
    }

    kernel_events::~kernel_events()
    {
        close();
    }

    void kernel_events::close()
    {
        if(fd == -1) 
            return;
        ::close(fd);
        fd = -1;
    }

    void kernel_events::control(
        const int fd,
        const int op, 
        const int flags, 
        event ev)
    {
        assert(fd != -1);
        struct epoll_event ep_ev = {
            .events = flags | ev.events,
            .data = { .u64 = ev.data.u64 } };
        if(::epoll_ctl(fd, op, fd, &ep_ev) == -1)
            throw system_error("::epoll_ctl");
    }

    int kernel_events::wait(const int timeout)
    {
        num_events = 0;
        position = 0;
        int n = -1;
        if((n = ::epoll_wait(fd, events.get(), max_events, timeout)) == -1)
            throw system_error("::epoll_wait");
        num_events = n;
        return n;
    }

    bool kernel_events::next(event &ev)
    {
        assert(0 <= position && position <= num_events);
        if(position == num_events) 
            return false;
        struct epoll_event *ep_ev = events.get() + position++;
        ev.events = ep_ev->events;
        ev.data.u64 = ep_ev->data.u64;
        return true;
    }

    int kernel_events::get_num_events()
    {
        return num_events;
    }

    int kernel_events::get_max_events()
    {
        return max_events;
    }

    bool kernel_events::empty()
    {
        assert(0 <= position && position <= num_events);
        return position == num_events;
    }
}
