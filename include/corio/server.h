#ifndef CORIO_SERVER_H
#define CORIO_SERVER_H

#include "corio/data.h"
#include "corio/kernel_events.h"
#include "corio/coroutine.h"

namespace corio 
{
    enum protocol {                             /** Network Protocol */
        PROTOCOL_UDP                = 1,        /** UDP over IPv4 */
        PROTOCOL_UDP6               = 2,        /** UDP over IPv6 */
        PROTOCOL_TCP                = 3,        /** TCP over IPv4 */
        PROTOCOL_TCP6               = 4,        /** TCP over IPv6 */
        PROTOCOL_NONE               = 5         /** No Protocol */
    };

    /** Server Task */
    struct task;

     /** RAII Coroutine Task State */
    template<typename A> using cor = coroutine<promise<task, A>>;

    /** Base Callback */
    using spawn_callback = cor<int> (*)(data state);

    /** Socket Accept Callback */
    using accept_callback = cor<int> (*)(event_fd &sock, data state);

    /** Error Callback */
    using error_callback = int (*)(int error, data state);

    struct server_params                        /** Server Parameters */
    {
        int protocol;                           /** Socket Protocol. */
        int port;                               /** Socket Port */
        int backlog;                            /** Socket Listen Backlog */

        int workers;                            /** Number of Worker Threads */
        int worker_tasks;                       /** Max Tasks per Worker */

        int server_timeout;                     /** Server Polling Timeout */
        int worker_timeout;                     /** Worker Polling Timeout */

        int server_events;                      /** Server Event Buffer */
        int worker_events;                      /** Worker Event Buffer */

        accept_callback on_accept;              /** Accept Callback */
        error_callback on_error;                /** Error Callback */

        data state;                             /** User State */

        server_params();                        /** Default Parameters */
    };

    /** Network Server Interface */
    struct server
    {
        /** 
         * Create a new server.
         */
        static server *create(server_params &params);

        /** 
         * Destroy the server.
         */
        virtual ~server() = default;

        /**
         * Initialize and set up all the system resources needed for server
         * operation.  
         * 
         * @return ERR_OK
         * @throw system_error
         */
        virtual int open() = 0;

        /**
         * Close the server, freeing all system resources acquired by open.
         * 
         * RETURNS: ERR_OK
         */
        virtual int close() = 0;

        /** 
         * Start the server by entering its main loop. 
         * 
         * @return ERR_OK
         * @throws system_error, runtime_error
         */
        virtual int start() = 0;

        /**
         * Signal the server to eventually stop. 
         * 
         * @return ERR_OK
         */
        virtual int stop() = 0;

        /** 
         * Spawn a new task. 
         * 
         * RETURNS: 
         *      ERR_OK - Task registered successfully.
         *      ERR_LIMIT - Task limit reached.
         *      ERR_WANTW - Could not dispatch to any workers.
         */
        virtual int spawn(spawn_callback call, data state) = 0;
    };

    /** 
     * Get the current worker's kernel event's object.
     */
    kernel_events &get_kernel_events();

    /** 
     * Control events for file descriptor from within a coroutine. 
     * 
     * @returns ERR_OK
     * @throws system_error
     */
    int control_events(
        event_fd &fd,
        int op,
        int flags,
        int events);

    /** 
     * Wait for events from within a coroutine.  
     * @returns >= 0 - The number of events received.
     */
    struct wait_for_io_events {
        uint64_t timeout;
        wait_for_io_events(uint64_t timeout);
        bool await_ready() const noexcept;
        void await_suspend(coroutine_handle<> handle);
        int await_resume() noexcept;
    };

    /**
     * IO Events Iterable.
     */
    struct io_events_type { 
        event *begin();
        event *end();
    } io_events;
}

#endif