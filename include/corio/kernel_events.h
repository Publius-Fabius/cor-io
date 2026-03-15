/** Shallow wrapper generalizing epoll/kqueue. */

#ifndef CORIO_KERNEL_EVENTS_H
#define CORIO_KERNEL_EVENTS_H

#include "corio/data.h"
#include <stdbool.h>
#include <memory>

#ifdef __linux__

#define EPOLL_EVENTS 1

#include <sys/epoll.h>

#elif   defined(__APPLE__) || \
        defined(__FreeBSD__) || \
        defined(__NetBSD__) || \
        defined(__OpenBSD__) || \
        defined(__DragonFly__)
#define KQUEUE_EVENTS 1
#error "KQUEUE NOT YET SUPPORTED"
#else
#error "SYSTEM NOT SUPPORTED"
#endif

#ifdef EPOLL_EVENTS

namespace corio 
{
    enum event_op {                                 /** Event Operation */
        EVENT_ADD               = EPOLL_CTL_ADD,    /** Add Events */
        EVENT_MODIFY            = EPOLL_CTL_MOD,    /** Modify Events */
        EVENT_DELETE            = EPOLL_CTL_DEL     /** Delete Events */
    }; 

    enum event_flag {                               /** Event Flag */
        EVENT_ONESHOT           = EPOLLONESHOT,     /** One Shot */
        EVENT_EDGE              = EPOLLET           /** Edge Triggered */
    };

    enum event_type {                               /** Event Type */
        EVENT_READ              = EPOLLIN,          /** Read Event */
        EVENT_WRITE             = EPOLLOUT,         /** Write Event */
        EVENT_ERROR             = EPOLLERR,         /** Error Event */
        EVENT_HANGUP            = EPOLLHUP,         /** Hang Up Event */
        EVENT_NOTIFY            = 1u << 18,         /** Wakeup Notification */
        EVENT_TIMEOUT           = 1u << 19          /** Timeout Event */
    };

#elif defined(KQUEUE_EVENTS)
#endif

    /** IO Event */
    struct event {
        int events;
        data data;
    };

    class kernel_events;

    /** RAII File Descriptor For Event Handling */
    struct event_fd {

        int fd = -1;
        int flags = 0;

#ifdef EPOLL_EVENTS
        int epfd = -1;      
#elif defined(KQUEUE_EVENTS)
#endif

        event_fd(kernel_events &evs, int fd);
        ~event_fd(); 
    };

    /** IO Event Object */
    class kernel_events
    {
        int fd, num_events, max_events, position;

#ifdef EPOLL_EVENTS
        std::unique_ptr<::epoll_event[]> events;           
#elif defined(KQUEUE_EVENTS)
#endif

        public:

        /** 
         * Construct a kernel_events that can handle max_events at once.
         * THROWS: system_error
         */
        kernel_events(const int max_events);
        ~kernel_events();

        /**
         * Close the kernel_events. 
         * THROWS: system_error
         */
        void close();

        /** 
         * Control events for file descriptor. 
         * THROWS: system_error
         */
        void control(
            event_fd &fd,
            const int op, 
            const int flags, 
            event event);
        
        /** 
         * Wait timeout milliseconds for new events. 
         * 
         * RETURNS: 0 to N - The number of events ready for inspection.
         * THROWS: system_error
         */
        int wait(const int timeout);

        /** Event Iterator */
        struct iterator {
            
#ifdef EPOLL_EVENTS
            epoll_event *pointer;
#elif defined(KQUEUE_EVENTS)
#endif
            event &operator*();
            iterator operator++();
            bool operator!=(iterator &other);
        };
 
        /** 
         * Iterator to first event. 
         */
        iterator begin();

        /** 
         * Iterator to last event.
         */
        iterator end();

        /** 
         * Get the current number of new events.
         */
        int get_num_events();

        /** 
         * Get the maximum number of events this object can process at once.
         */
        int get_max_events();
    };
}

#endif