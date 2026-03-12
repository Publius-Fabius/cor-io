#include "corio/server.h"
#include "corio/slot_map.h"
#include "corio/nonblocking_pipe.h"
#include <queue>
#include <vector>
#include <memory>
#include <thread>

namespace corio 
{
    server_params::server_params()
    {
        protocol              = PROTOCOL_TCP;
        port                  = 80;
        backlog               = 8;

        workers               = 1;  
        worker_tasks          = 4;

        worker_timeout        = 5000;
        server_timeout        = 5000;

        server_events         = 32;
        worker_events         = 32;
        
        on_error              = NULL;
        on_accept             = NULL;

        state                = { .u64 = 0 };
    }

    enum server_mode {                              /** Server Mode */
        MODE_CREATED         = 1,                   /** Server Created */
        MODE_RUNNING         = 2,                   /** Server Running */
        MODE_STOPPING        = 3,                   /** Server Stopping */
        MODE_STOPPED         = 4                    /** Server Stopped */
    };

    struct task {
        coroutine handle;                           /** Coroutine Handle */
        int mark;                                   /** Ready Set Bit */
    };

    enum message_type {                             /** Message Type */
        MSG_SHUTDOWN        = 1,                    /** Prepare Shutdown */
        MSG_WAKEUP          = 2                     /** Wakeup Notification */
    };

    struct message {                                /** Message */
            int type;                               /** Message Type */
            data data;                              /** Message Data */
    };

    struct standard_server;

    /** Worker Thread */
    struct worker 
    {
        struct timeout_queue_elem {
            uint64_t expiry;
            int slot;
            uint32_t epoch;
            bool operator <(struct timeout_queue_elem &elem) {
                return this->expiry > elem.expiry;
            }
        };

        using timeout_queue = std::priority_queue<timeout_queue_elem>;
        using message_queue = nonblocking_pipe<message>;
        using socket_queue = nonblocking_pipe<int>;
        using task_map = slot_map<task>;
        using ready_set = std::vector<task*>;

        int id;
        int mode;

        standard_server &server;
        timeout_queue timeouts;
        kernel_events events;
        task_map slots;
        message_queue messages;
        socket_queue pending;
        ready_set ready;
        task *active;

        worker(standard_server &srv);
        ~worker();
    };
    
    struct standard_server : server
    {
        using worker_set = std::vector<worker>;
        using thread_set = std::vector<std::thread>;
        
        server_params params;
        int socket;
        int mode;
        worker_set workers; 
        thread_set threads;
        kernel_events events;

        standard_server();
        ~standard_server();

        int open() override;
        int close() override;
        int start() override;
        int stop() override;
        int spawn(callback call, data state) override;
    };

    worker::worker(standard_server &srv) :
        server(srv),
        events(srv.params.worker_events),
        active(NULL) { }

    worker::~worker() = default;
    
}
