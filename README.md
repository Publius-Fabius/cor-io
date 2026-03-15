## COR-IO - A C++ Coroutine Based Network Server

A high-performance network framework leveraging C++20 Coroutines and Symmetric Transfer to provide a clean API for non-blocking I/O on POSIX compatible systems.

### Architecture

This project implements a high-performance, multi-threaded network server architecture designed for maximum throughput and deterministic resource management. By combining a Single-Producer, Multiple-Consumer (SPMC) reactor pattern with C++20's lightweight coroutines, the server achieves a "Never-Drop" guarantee for connections while maintaining a non-blocking execution pipeline.  

The Reactor: The reactor thread manages the listening socket and performs global task accounting. It uses an explicit backpressure mechanism: before accepting a connection, it "pre-bills" a global atomic task counter. If the server is at capacity, the reactor ceases to accept() new sockets, allowing the kernel's TCP backlog to handle the pressure. This ensures that memory is never over-allocated and connections are never dropped unexpectedly due to user-space overflow.

The Workers: Tasks are distributed to workers using a round-robin "work-seeking" algorithm via kernel pipes, mitigating the thundering herd problem under normal conditions. This design also uses the pipe's internal buffer's physical limit as a natural backpressure signaling mechanism.  Workers operate as independent coroutine executors, utilizing a slot_map for O(1) task access and a generation-based versioning system to prevent "stale" pointer bugs during high-frequency task recycling.

### Roadmap

Currently vibe porting core logic from the original C implementation. Future modules include:
- [ ] HTTP/1.1 Protocol Suite: Fully asynchronous request/response parsing.
- [ ] SSL/TLS Support: Integration with non-blocking encryption bio-pairs.
- [ ] WebSockets: Full-duplex communication over the existing coroutine architecture.
- [ ] BSD/Mac kqueue support (currently only supports linux epoll events).