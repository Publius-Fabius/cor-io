# COR-IO - A C++ Coroutine Based Network Server

A high-performance, asynchronous network framework leveraging C++20 Coroutines and Symmetric Transfer to provide a clean, imperative-style API for non-blocking I/O.

## Overview

This project provides a modern C++ wrapper around low-level kernel event primitives. By utilizing coroutines, it eliminates the "callback hell" typically associated with asynchronous programming, allowing complex network logic (like SSL handshakes and HTTP parsing) to be written as straight-line code.

## Roadmap

Currently vibe porting core logic from the original C implementation. Future modules include:
- [ ] HTTP/1.1 Protocol Suite: Fully asynchronous request/response parsing.
- [ ] SSL/TLS Support: Integration with non-blocking encryption bio-pairs.
- [ ] WebSockets: Full-duplex communication over the existing coroutine architecture.