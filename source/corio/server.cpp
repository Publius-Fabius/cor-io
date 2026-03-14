#include "corio/server.h"
#include "corio/slot_map.h"
#include "corio/nonblocking_pipe.h"
#include "corio/time.h"
#include <queue>
#include <vector>
#include <memory>
#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>
#include <stdatomic.h>

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

    enum message_type {                             /** Message Type */
        MESSAGE_SHUTDOWN    = 1,                    /** Prepare Shutdown */
        MESSAGE_NOTIFY      = 2                     /** Wakeup Notification */
    };

    struct message {                                /** Message */
            int type;                               /** Message Type */ 
            int slot;                               /** Task Slot Number */
            uint64_t id;                            /** Task Identifier */
    };

    struct task {
        int mark;                                   /** Ready Set Bit */
        uint64_t id;                                /** Task Id */
        coroutine handle;                           /** Coroutine Handle */
        std::vector<event> events;                  /** Event Set */
        task(uint64_t id_, coroutine handle_) : 
            id(id_), 
            handle(handle_), 
            mark(0) { }
    };

    struct standard_server;

    /** Worker Thread */
    struct worker 
    {
        struct timeout_queue_elem {
            int slot;
            uint64_t expiry;
            uint64_t generation;
            bool operator <(struct timeout_queue_elem &elem) {
                return expiry > elem.expiry;
            }
        };

        struct task_queue_elem {
            spawn_callback callback;
            data state;
        };

        struct ready_set {
            std::vector<task*> tasks;
            void push(task *t) { 
                assert(t);
                if(t->mark) 
                    return;
                t->mark = 1;
                tasks.push_back(t); 
            }
            task *pop() {
                if(tasks.empty()) 
                    return NULL;
                task *t = tasks.back();
                tasks.pop_back();
                assert(t);
                assert(t->mark);
                t->mark = 0;
                return t;
            }
            bool empty() {
                return tasks.empty();
            }
            void clear() {
                tasks.clear();
            }
        };

        using timeout_queue = std::priority_queue<timeout_queue_elem>;
        using message_queue = nonblocking_pipe<message>;
        using task_queue = nonblocking_pipe<task_queue_elem>;
        using task_map = slot_map<task>;

        int id;
        int mode;

        standard_server &server;
        timeout_queue timeouts;
        kernel_events events;
        task_map slots;
        message_queue messages;
        task_queue pending;
        ready_set ready;
        task *active;

        worker(standard_server &srv);
        ~worker();

        void enqueue_ready_set();
        void dispatch_ready_set();
        bool tick();
        void start();
    };
    
    struct standard_server : server
    {
        using worker_set = std::vector<worker>;
        using thread_set = std::vector<std::jthread>;
        
        server_params params;
        int server_socket;
        atomic_int32_t mode;
        atomic_int32_t num_tasks;
        atomic_uint64_t next_id;
        worker_set workers; 
        thread_set threads;
        kernel_events events;

        standard_server(server_params &ps);
        ~standard_server();

        void create_server_socket();
        void bind_server_socket();
        void listen_on_server_socket();

        int open() override;
        int close() override;
        void dispatch_connection(const int socket);
        void accept_connections();
        void main_loop();
        int start() override;
        int stop() override;
        int spawn(spawn_callback call, data state) override;
    };

    thread_local worker *current_worker = NULL;

    struct fd_slot_pair {
        union { 
            struct {
                int fd;
                int slot;
            };
            data pack;
        };
    };

    worker::worker(standard_server &srv) :
        server(srv),
        events(srv.params.worker_events),
        active(NULL) { }

    worker::~worker() = default;

    void worker::enqueue_ready_set()
    {
        const auto now = now_ms<uint64_t>();

        while(!timeouts.empty()) {
            auto ent = timeouts.top();
            if(ent.expiry > now) break;
            timeouts.pop();
            auto *t = slots.with_generation(ent.slot, ent.generation);
            if(!t) continue;
            ready.push(t);
            t->events.push_back({ EVENT_TIMEOUT, { 0 }});
        }

        for(event ev; events.next(ev);) {
            fd_slot_pair pair { .pack = ev.data };
            auto *t = slots.get_pointer(pair.slot);
            if(!t) continue;
            ready.push(t);
            t->events.push_back({ ev.events, { .fd = pair.fd }});
        }
        
        for(message msg; messages.read(msg) == ERR_OK;) {
            switch(msg.type) {
                case MESSAGE_SHUTDOWN: continue;
                case MESSAGE_NOTIFY:
                    auto *t = slots.get_pointer(msg.slot);
                    if(!t) continue;
                    if(t->id != msg.id) continue;
                    ready.push(t);
                    t->events.push_back({ EVENT_NOTIFY, { 0 }});
                    continue;
                default: std::terminate;
            }
        }

        for(task_queue_elem elem; pending.read(elem) == ERR_OK;) {
            task *t = new task(
                ::atomic_fetch_add(&server.next_id, 1),
                elem.callback(server.params.state));
            ready.push(t);
        }
    }

    void worker::dispatch_ready_set()
    {
        
    }

    bool worker::tick()
    {
        uint64_t timeout = server.params.worker_timeout;

        if(!timeouts.empty()) {
            uint64_t now = now_ms<uint64_t>();
            uint64_t min = timeouts.top().expiry;
            uint64_t wake = min - now;
            timeout = min <= now ? 0 : (timeout < wake ? timeout : wake);
        }
            
        events.wait((int)timeout);
        
        enqueue_ready_set();
        dispatch_ready_set();

        return 
            atomic_load(&server.mode) == MODE_RUNNING && 
            atomic_load(&server.num_tasks) > 0;
    }
    
    static void launch_worker(worker &worker) 
    {
        while(worker.tick());
    }

    standard_server::standard_server(server_params &ps) :
        params(ps),
        events(ps.server_events),
        server_socket(-1),
        mode(MODE_CREATED)
    {
        for(int x = 0; x < ps.workers; ++x)
            workers.emplace_back(*this);
    }

    standard_server::~standard_server()
    {
        close();
    }
 
    void standard_server::create_server_socket()
    {
        assert(server_socket == -1);

        int domain = -1;
        int type = -1;
        
        switch(params.protocol) {
            case PROTOCOL_TCP: 
                domain = AF_INET;
                type = SOCK_STREAM;
                break;
            case PROTOCOL_TCP6:
                domain = AF_INET6;
                type = SOCK_STREAM;
                break;
            case PROTOCOL_UDP:
                domain = AF_INET;
                type = SOCK_DGRAM;
                break;
            case PROTOCOL_UDP6:
                domain = AF_INET6;
                type = SOCK_DGRAM;
                break;
            default: 
                std::terminate();
        }

        const int fd = ::socket(domain, type, 0);
        if(fd == -1) 
            throw system_error("::socket");
        
        const int flags = ::fcntl(fd, F_GETFL);
        if(flags == -1) {
            ::close(fd);
            throw system_error("::fcntl");
        }
        if(::fcntl(fd, F_SETFL, O_NONBLOCK | flags) == -1) {
            ::close(fd);
            throw system_error("::fcntl");
        }
        
        server_socket = fd;
    }

    void standard_server::bind_server_socket()
    {
        assert(server_socket != -1);

        struct sockaddr_storage addr;
        ::memset(&addr, 0, sizeof(struct sockaddr_storage));
        struct sockaddr_in *in = (struct sockaddr_in*)&addr;
        struct sockaddr_in6 *in6 = (struct sockaddr_in6*)&addr;
        socklen_t addrlen;

        switch(params.protocol) {
            case PROTOCOL_TCP: 
                addrlen = sizeof(struct sockaddr_in);
                in->sin_family = AF_INET;
                in->sin_port = ::htons((uint16_t)params.port);
                in->sin_addr.s_addr = ::htonl(INADDR_ANY);
                break;
            case PROTOCOL_TCP6:
                addrlen = sizeof(struct sockaddr_in6);
                in6->sin6_family = AF_INET6;
                in6->sin6_port = ::htons((uint16_t)params.port);
                in6->sin6_addr = in6addr_any;
                break;
            case PROTOCOL_UDP:
                addrlen = sizeof(struct sockaddr_in);
                in->sin_family = AF_INET;
                in->sin_port = ::htons((uint16_t)params.port);
                in->sin_addr.s_addr = ::htonl(INADDR_ANY);
                break;
            case PROTOCOL_UDP6:
                addrlen = sizeof(struct sockaddr_in6);
                in6->sin6_family = AF_INET6;
                in6->sin6_port = ::htons((uint16_t)params.port);
                in6->sin6_addr = in6addr_any;
                break;
            default: 
                std::terminate();
        }

        if(::bind(server_socket, (struct sockaddr*)&addr, addrlen) == -1)
            throw system_error("::bind"); 
    }

    void standard_server::listen_on_server_socket()
    {
        assert(server_socket != -1);
        if(::listen(server_socket, params.backlog) == -1) 
            throw system_error("::listen");
    }

    int standard_server::open()
    {
        assert(server_socket == -1);
        create_server_socket();
        bind_server_socket();
        listen_on_server_socket();
        events.control(server_socket, EVENT_ADD, 0, { EVENT_READ, { 0 }});
        return 0;
    }

    int standard_server::close()
    {
        if(server_socket == -1) 
            return 0;
        ::close(server_socket);
        server_socket = -1;
        return 0;
    }

    coroutine accept_trampoline(data state)
    {
        assert(current_worker != NULL);

        // current_worker is thread_local set in the worker thread.
        server_params &params = current_worker->server.params;

        return params.on_accept(state.fd, params.state);
    }

    void standard_server::dispatch_connection(const int fd)
    {
        do {
            switch(spawn(accept_trampoline, { .fd = fd })) {
                case ERR_WANTW: break;
                case ERR_OK: return;
                default: std::terminate();
            }

            // Stop listening to server_socket.
            event ev { EVENT_READ, { 0 } };
            events.control(server_socket, EVENT_DELETE, 0, ev);

            // Listen for write events on pending queues.
            ev.events = EVENT_WRITE;
            for(auto &w : workers) 
                events.control(w.pending.get_write_fd(), EVENT_ADD, 0, ev);
 
            // Wait for a pipe to be writable.
            while(events.wait(1000) == 0)
                // Log significant backpressure.
                if(params.on_error(ERR_BACK, params.state) != ERR_OK)
                    throw runtime_error("on_error");

            // Stop listening for write events on pending queues.
            for(auto &w : workers) 
                events.control(w.pending.get_write_fd(), EVENT_DELETE, 0, ev);

            // Start listening to server_socket again.
            ev.events = EVENT_READ;
            events.control(server_socket, EVENT_ADD, 0, ev);

        } while(true);
    }

    void standard_server::accept_connections()
    {
        const int max_tasks = params.worker_tasks * params.workers;
        
        while(::atomic_load(&num_tasks) < max_tasks) {

            const int fd = ::accept(server_socket, NULL, NULL);
            if(fd == -1) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) 
                    return;
                const int err = errno;
                if(params.on_error(ERR_SYS, params.state) != ERR_OK)
                    throw runtime_error("on_error");
                switch(err) {
                    case EHOSTUNREACH:
                    case ENETUNREACH:
                    case ENONET:
                    case EHOSTDOWN:
                    case ENETDOWN:
                        return;
                    case EPROTO:
                    case ENOPROTOOPT:
                    case ECONNABORTED:
                    case EOPNOTSUPP:
                        continue;
                    default:
                        throw system_error("on_error");
                }
            }

            const int flags = ::fcntl(fd, F_GETFL);
            if(flags == -1) {
                ::close(fd);
                throw system_error("::fcntl");
            }
            if(::fcntl(fd, F_SETFL, O_NONBLOCK | flags) == -1) {
                ::close(fd);
                throw system_error("::fcntl");
            } 

            ::atomic_fetch_add(&num_tasks, 1);

            dispatch_connection(fd);
        }
    }

    void standard_server::main_loop()
    {
        while(::atomic_load(&mode) == MODE_RUNNING) {
            accept_connections();
            events.wait(params.server_timeout);
        }

        threads.clear(); // joins jthreads

        ::atomic_store(&mode, MODE_STOPPED);
    }

    int standard_server::start()
    {
        assert(::atomic_load(&mode) == MODE_CREATED);
        assert(server_socket != -1);
        ::atomic_store(&mode, MODE_RUNNING);
        for(int x = 0; x < workers.size(); ++x) {
            threads.emplace_back(launch_worker, workers[x]);
        }
        main_loop();
        return 0;
    }

    int standard_server::stop()
    {
        assert(::atomic_load(&mode) == MODE_RUNNING);
        ::atomic_store(&mode, MODE_STOPPING);
        return 0;
    }

    int standard_server::spawn(spawn_callback call, data state)
    {
        static atomic_size_t seed = 0;

        worker::task_queue_elem elem = { call, state };
          
        const size_t size = workers.size();
        const size_t base_index = ::atomic_fetch_add(&seed, 1);

        for(size_t x = 0; x < size; ++x) {
            const size_t index = (x + base_index) % size;
            switch(workers[index].pending.write(elem)) {
                case ERR_WANTW: continue;
                case ERR_OK: return ERR_OK;
                default: std::terminate();
            }
        }

        return ERR_WANTW;
    }
}
