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
    server_params::server_params() {
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
        int slot;                                   /** Slot Number */
        uint64_t id;                                /** Task Id */
        cor<int> runner;                            /** Coroutine */
        std::vector<event> events;                  /** Event Set */
        task(uint64_t id_, cor<int> &&runner_) : 
            id(id_), 
            runner(std::move(runner_)), 
            mark(0) { }
    };

    struct standard_server;

    /** Worker Thread */
    struct worker {
        struct timeout_queue_elem {
            int slot;
            uint64_t expiry;
            uint64_t generation;
            timeout_queue_elem(int slot_, uint64_t exp, uint64_t gen) :
                slot(slot_), expiry(exp), generation(gen) { }
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
                if(t->mark) return;
                t->mark = 1;
                tasks.push_back(t); 
            }
            task *pop() {
                if(tasks.empty()) return NULL;
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
        task *current_task;

        worker(standard_server &srv);
        ~worker();

        void enqueue_ready_set();
        void dispatch_ready_set();
        task *create_task(spawn_callback callback, data state);
        void delete_task(task *t);
        bool tick();
        void start();
    };
    
    struct standard_server : server
    {
        using worker_set = std::vector<worker>;
        using thread_set = std::vector<std::jthread>;
        
        server_params params;
        event_fd server_socket;
        atomic_int32_t mode;
        atomic_int32_t num_tasks;
        atomic_uint64_t next_id;
        worker_set workers; 
        thread_set threads;
        kernel_events events;

        standard_server(server_params &ps);
        ~standard_server();

        bool increment_tasks();
        void decrement_tasks();

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
        int spawn_dispatch(spawn_callback call, data state);
        int spawn(spawn_callback call, data state) override;
    };

    thread_local worker *current_worker = nullptr;

    struct fd_slot_pair {
        fd_slot_pair(int fd_, int slot_) :
            fd(fd_), slot(slot_) { }
        fd_slot_pair(data pack_) :
            pack(pack_) { }
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
        pending(events),
        messages(events),
        current_task(nullptr) { }

    worker::~worker() {
        for(task *t : slots) {
            delete t;
            server.decrement_tasks();
        }
    }

    void worker::enqueue_ready_set() {
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

        for(event ev : events) {
            fd_slot_pair pair(ev.data);
            auto *t = slots[pair.slot];
            if(!t) continue;
            ready.push(t);
            t->events.push_back({ ev.events, { .fd = pair.fd }});
        }
        
        for(message msg; messages.read(msg) == ERR_OK;) {
            switch(msg.type) {
                case MESSAGE_SHUTDOWN: continue;
                case MESSAGE_NOTIFY:
                    auto *t = slots[msg.slot];
                    if(!t) continue;
                    if(t->id != msg.id) continue;
                    ready.push(t);
                    t->events.push_back({ EVENT_NOTIFY, { 0 }});
                    continue;
                default: std::terminate();
            }
        }

        for(task_queue_elem elem; pending.read(elem) == ERR_OK;) {
            auto *t = create_task(elem.callback, server.params.state);
            ready.push(t);
        }
    }

    void worker::dispatch_ready_set() {
        for(task *t; t = ready.pop();) {
            assert(!t->runner.handle.done());

            slots.increment_generation(t->slot);
            current_task = t;
            t->runner.handle.resume();
            current_task = nullptr;

            if(t->runner.handle.done()) delete_task(t);
        }
    }

    task *worker::create_task(spawn_callback callback, data state) {
        task *t = new task(
                ::atomic_fetch_add(&server.next_id, 1),
                callback(state));
        t->runner.handle.promise().state = t;
        t->slot = slots.acquire(t);
        return t;
    }

    void worker::delete_task(task *t) {
        slots.release(t->slot);
        delete t;
        server.decrement_tasks();
    }

    bool worker::tick() {
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
    }
    
    static void launch_worker(worker &w) {
        standard_server &s = w.server;
        current_worker = &w;
        while(  ::atomic_load(&s.mode) != MODE_STOPPED || 
                ::atomic_load(&s.num_tasks) > 0)
            w.tick();
    }

    standard_server::standard_server(server_params &ps) :
        params(ps),
        events(ps.server_events),
        server_socket(events, event_fd::unique()),
        mode(MODE_CREATED)
    {
        for(int x = 0; x < ps.workers; ++x)
            workers.emplace_back(*this);
    }

    standard_server::~standard_server() {
        close();
    }
 
    bool standard_server::increment_tasks() {
        const int32_t max_tasks = params.worker_tasks * params.workers;
        int32_t num_tasks_ = ::atomic_load(&num_tasks);
        for(;;) {
            int32_t next = num_tasks_ + 1;
            if(next > max_tasks) return false;
            if(::atomic_compare_exchange_weak(
                &num_tasks, &num_tasks_, next)) 
                return true;
        }
    }
    
    void standard_server::decrement_tasks() {
        ::atomic_fetch_sub(&num_tasks, 1);
    }

    void standard_server::create_server_socket() {

        assert(server_socket);

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
        
        server_socket.assign(fd);

        const int flags = ::fcntl(fd, F_GETFL);
        if(flags == -1) 
            throw system_error("::fcntl");
        if(::fcntl(fd, F_SETFL, O_NONBLOCK | flags) == -1) 
            throw system_error("::fcntl");
    }

    void standard_server::bind_server_socket() {
        assert(!server_socket);

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

        if(::bind(*server_socket, (struct sockaddr*)&addr, addrlen) == -1)
            throw system_error("::bind"); 
    }

    void standard_server::listen_on_server_socket() {
        assert(!server_socket);
        if(::listen(*server_socket, params.backlog) == -1) 
            throw system_error("::listen");
    }

    int standard_server::open() {
        create_server_socket();
        bind_server_socket();
        listen_on_server_socket();
        events.control(server_socket, EVENT_ADD, 0, { EVENT_READ, { 0 }});
        return 0;
    }

    int standard_server::close() { 
        return 0;
    }

    cor<int> accept_trampoline(data state) {
        assert(current_worker != nullptr);

        // current_worker is a thread_local set in the worker thread.
        server_params &params = current_worker->server.params;

        // Create an automatically managed epoll_fd.
        event_fd efd(current_worker->events, event_fd::unique(), state.fd);

        // Transfer control over to user.
        co_await params.on_accept(efd, params.state);
        
        co_return ERR_OK;
    }

    void standard_server::dispatch_connection(const int fd) {
        do {
            switch(spawn_dispatch(accept_trampoline, { .fd = fd })) {
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
                events.control(w.pending.writer, EVENT_ADD, 0, ev);
 
            // Wait for a pipe to be writable.
            while(events.wait(1000) == 0)

                // Log significant backpressure events.     
                params.on_error(ERR_BACK, params.state);

            // Stop listening for write events on pending queues.
            for(auto &w : workers) 
                events.control(w.pending.writer, EVENT_DELETE, 0, ev);

            // Start listening to server_socket again.
            ev.events = EVENT_READ;
            events.control(server_socket, EVENT_ADD, 0, ev);

        } while(true);
    }

    void standard_server::accept_connections() {
        const int max_tasks = params.worker_tasks * params.workers;
            
        do {
            // "Pre-allocate" the task so we aren't stuck with an open fd. 
            if(!increment_tasks())
                return;

            const int fd = ::accept(server_socket, NULL, NULL);
            if(fd == -1) {
                
                // Accept failed, free up the space for another task.
                decrement_tasks();

                // The server socket had no awaiting connections.
                if(errno == EAGAIN || errno == EWOULDBLOCK) 
                    return;

                // Store errno because on_error may clobber it.
                const int err = errno;

                // Log the error.
                params.on_error(ERR_SYS, params.state);
            
                switch(err) {

                    // These are temporary problems that can be resolved.
                    case EHOSTUNREACH:
                    case ENETUNREACH:
                    case ENONET:
                    case EHOSTDOWN:
                    case ENETDOWN:
                        return;

                    // These are problems with the client.
                    case EPROTO:
                    case ENOPROTOOPT:
                    case ECONNABORTED:
                    case EOPNOTSUPP:
                        continue;

                    // These problems can't be resolved.
                    default:
                        throw system_error("::accept");
                }
            }

            const int flags = ::fcntl(fd, F_GETFL);
            if(flags == -1) {
                ::close(fd);
                decrement_tasks();
                throw system_error("::fcntl");
            }
            if(::fcntl(fd, F_SETFL, O_NONBLOCK | flags) == -1) {
                ::close(fd);
                decrement_tasks();
                throw system_error("::fcntl");
            } 

            dispatch_connection(fd);

        } while((::atomic_load(&mode) == MODE_RUNNING));
    }

    void standard_server::main_loop() {
        while(::atomic_load(&mode) == MODE_RUNNING) {
            accept_connections();
            events.wait(params.server_timeout);
        }
    }

    int standard_server::start() {

        assert(::atomic_load(&mode) == MODE_CREATED);
        assert(::atomic_load(&num_tasks) == 0);
        assert(server_socket);

        ::atomic_store(&mode, MODE_RUNNING);

        // Launch the worker threads.
        for(size_t x = 0; x < workers.size(); ++x) 
            threads.emplace_back(launch_worker, workers[x]);
        
        // Enter main reactor loop.
        try {
            main_loop();
        } catch (std::exception &ex) {
            fputs(ex.what(), stderr);
        }
        
        ::atomic_store(&mode, MODE_STOPPED);

        // Clearing here will automatically join the worker threads.
        threads.clear();

        return ERR_OK;
    }

    int standard_server::stop() {
        ::atomic_store(&mode, MODE_STOPPING);
        return ERR_OK;
    }

    int standard_server::spawn_dispatch(spawn_callback call, data state) {

        // Atomically incrementing seed for load balancing.
        static atomic_size_t seed = 0;

        worker::task_queue_elem elem = { call, state };
        
        // Size never changes so this is fine.  In the case of server shutdown,
        // the worker thread waits until all tasks are finished before stopping.
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

    int standard_server::spawn(spawn_callback call, data state) {
        if(!increment_tasks()) return ERR_LIMIT;
        int err = -1;
        if((err = spawn_dispatch(call, state)) != ERR_OK) {
            decrement_tasks();
            return err;
        }
        return ERR_OK;
    }

    kernel_events& get_kernel_events() {
        return current_worker->events;
    }

    int control_events(
        event_fd &fd,
        int op,
        int flags,
        int events) 
    {
        auto &w = *current_worker;
        auto &t = *w.current_task;
        fd_slot_pair pair(fd, t.slot);
        w.events.control(fd, op, flags, { events, { pair.pack }});
        return 0;
    }
    
    wait_for_io_events::wait_for_io_events(uint64_t timeout_) :
        timeout(timeout_) { }

    bool wait_for_io_events::await_ready() const noexcept {
        return true;
    }

    void wait_for_io_events::await_suspend(coroutine_handle<> handle) {
        auto &w = *current_worker;
        auto &t = *w.current_task;
        auto expiry = timeout + now_ms<uint64_t>();
        auto gen = w.slots.get_generation(t.slot);
        w.timeouts.push(worker::timeout_queue_elem(t.slot, expiry, gen));
        t.events.clear();
    }

    int wait_for_io_events::await_resume() noexcept {
        auto &t = *current_worker->current_task;
        return (int)t.events.size();
    }

    event* io_events_type::begin() {
        auto &t = *current_worker->current_task;
        return t.events.data();
    }

    event* io_events_type::end() {
        auto &t = *current_worker->current_task;
        return t.events.data() + t.events.size();
    }
}
