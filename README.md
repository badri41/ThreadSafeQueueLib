<div align="center">

# ThreadSafeQueueLib
**A High-Performance Concurrent Queue Library in Modern C++**

[![C++23](https://img.shields.io/badge/C++-23-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B23)
[![CMake](https://img.shields.io/badge/CMake-Build-success.svg)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

*Designed and Developed by **[Badri Bishal Das](https://github.com/badri41)** — DSAI Undergrad @ IIT Guwahati | Incoming ASD Intern @ Google*

[📚 Read the Engineering Blog Post](https://www.badribishaldas.in/blog/threadSafeQueue/)

</div>

---

## 📌 Overview

**ThreadSafeQueueLib** is a header-only, highly optimized concurrent queue library written in Modern C++ (C++23). It provides a family of thread-safe queues supporting various concurrency models—including **MPMC** (Multi-Producer Multi-Consumer), **MPSC**, and **SPSC** topologies. 

A standard queue interface becomes fragile under thread contention. Something as ordinary as checking `empty()` before `front()` and `pop()` is prone to data races. ThreadSafeQueueLib solves this by offering both **Lock-Free** and **Blocking (Mutex-based)** implementations tailored for low latency and high throughput under heavy contention.

---

## 🚀 Features

- **Comprehensive Concurrency Models**:
  - **SPSC** (Single-Producer Single-Consumer) - Bounded & Unbounded, Lock-Free
  - **MPSC** (Multi-Producer Single-Consumer) - Unbounded, Lock-Free
  - **MPMC** (Multi-Producer Multi-Consumer) - Bounded & Unbounded, Lock-Free & Blocking
- **Template Metaprogramming**: Extensively uses C++ templates and concepts for compile-time specialization and optimization.
- **Lock-Free Semantics**: Implements robust non-blocking enqueue/dequeue operations using Compare-And-Swap (CAS) and atomic synchronization.
- **Hardware-Aware Optimizations**:
  - **False Sharing Mitigation**: Cache-line alignment (`alignas`) ensures producer and consumer indices do not share cache lines, preventing cache invalidation storms via the MESI protocol.
  - **Strict Memory Ordering**: Carefully crafted `std::memory_order_acquire`, `release`, and `relaxed` semantics to guarantee correctness without the overhead of full sequential consistency (`seq_cst`).
- **Benchmarked**: High-resolution `std::chrono` benchmarking against standard `std::mutex` and `std::condition_variable` implementations.

---

## 🏗️ Design Decisions & Architecture

### Why Lock-Free?
Mutex-based queues are easier to write and reason about. However, the lock-free route provides distinct advantages in system-level programming by avoiding thread suspension, context switching, and priority inversion. ThreadSafeQueueLib explores exact progress guarantees, contention handling, and the bleeding edge where performance meets correctness.

### Bounded vs. Unbounded
- **Bounded Queues**: Memory is fixed and predictable, making them extremely fast and cache-friendly, operating mostly over contiguous memory (ring buffers).
- **Unbounded Queues**: Solves the full-buffer problem dynamically but introduces allocation and reclamation complexities (ABA problem, node stubs, memory lifecycle).

### C++ Memory Model
Using `std::memory_order_seq_cst` everywhere is safe but severely limits performance. ThreadSafeQueueLib relies heavily on the acquire-release memory model, ensuring that writes in a producer thread mathematically *happen-before* the reads in a consumer thread, with relaxed operations used exclusively where cross-thread synchronization is unnecessary.

---

## 💻 Getting Started

### Prerequisites
- A C++23 compliant compiler (GCC 13+, Clang 16+, MSVC 19.38+)
- CMake (3.14+)

### Integration
ThreadSafeQueueLib is designed to be easily integrated into CMake projects. You can add it using `FetchContent` or by dropping it into your source tree.

```cmake
# Using FetchContent
include(FetchContent)
FetchContent_Declare(
    tsqlib
    GIT_REPOSITORY https://github.com/badri41/ThreadSafeQueueLib.git
    GIT_TAG main
)
FetchContent_MakeAvailable(tsqlib)

# Link against your executable
target_link_libraries(your_target PRIVATE ThreadSafeQueueLib)
```

### Usage Example

```cpp
#include <iostream>
#include <thread>
#include <tsfqueue.hpp> // Main unified header

int main() {
    // Instantiate a lock-free Single-Producer Single-Consumer unbounded queue
    tsfqueue::lockfree_spsc_unbounded<int> queue;

    // Producer Thread
    std::thread producer([&]() {
        for (int i = 0; i < 1000; ++i) {
            queue.push(i);
        }
    });

    // Consumer Thread
    std::thread consumer([&]() {
        int value;
        for (int i = 0; i < 1000; ++i) {
            // Spin until an item is successfully popped
            while (!queue.try_pop(value)) {
                std::this_thread::yield(); 
            }
            // Process value...
        }
    });

    producer.join();
    consumer.join();

    std::cout << "All elements processed successfully in a thread-safe manner!\n";
    return 0;
}
```

---

## 🧪 Testing & Benchmarking

The library uses **GoogleTest (gtest)** for rigorous unit testing and includes comprehensive correctness checks for data races (tested using Clang ThreadSanitizer `-fsanitize=thread`).

To build and run the tests:

```bash
mkdir build && cd build
cmake ..
cmake --build . -j4
ctest --output-on-failure -j4
```

Benchmarks measuring throughput and scalability across varying thread counts can be found in the `benchmarking/` directory.

---

## 🤝 Acknowledgements

This project was built under the mentorship and guidance of **Toshit Bhaiya** as part of the **Coding Club, IIT Guwahati**.

- **Author**: Badri Bishal Das
- **Blog Write-up**: [That time I got reincarnated as a lock-free queue library implementer](https://www.badribishaldas.in/blog/threadSafeQueue/)
- **Contact**: badribishaldas3000@gmail.com | [LinkedIn](https://www.linkedin.com/in/badri41/)

---

<div align="center">
  <i>"Debugging concurrency is not trivial. It requires stronger OS and architecture knowledge to reason cleanly about what the queue is doing under the hood."</i>
</div>
