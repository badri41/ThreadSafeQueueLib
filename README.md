<div align="center">

# ThreadSafeQueueLib
**Exploring Concurrency and Lock-Free Data Structures in Modern C++**

[![C++23](https://img.shields.io/badge/C++-23-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B23)
[![CMake](https://img.shields.io/badge/CMake-Build-success.svg)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

*A group learning project by **[Badri Bishal Das](https://github.com/badri41)** , **[Sujal Patnaik](https://github.com/Sujal-Patnaik)** and peers at IIT Guwahati*

[📚 Read the Engineering Blog Post](https://www.badribishaldas.in/blog/threadSafeQueue/)

</div>

---

## 👋 Hello!

**ThreadSafeQueueLib** is a group project created to dive deep into C++ and concurrency concepts. Instead of just reading about threads, locks, and atomics, we decided to build a family of thread-safe queues from scratch. 

Our goal wasn't to write the next big production library, but to really understand what makes concurrent programming so challenging and rewarding. A standard `std::queue` isn't safe to use when multiple threads are pushing and popping at the same time. Even a simple `empty()` check followed by a `pop()` can cause a data race. To figure out how to solve this under the hood, we implemented a variety of both blocking (mutex-based) and lock-free queues.

---

## 🛠️ What We Built

We explored different queue designs depending on how many threads are involved:
- **SPSC** (Single-Producer Single-Consumer) - Lock-free versions (both bounded and unbounded).
- **MPSC** (Multi-Producer Single-Consumer) - Unbounded lock-free.
- **MPMC** (Multi-Producer Multi-Consumer) - Both traditional locking and lock-free implementations.

We used C++ templates to write this as a header-only library, allowing the compiler to optimize the queues at build time depending on the types of data being stored.

---

## 🧠 Things We Learned

Building this taught us a lot of practical system-level concepts that are hard to grasp just from textbooks:

### 1. Why go Lock-Free?
Writing a concurrent queue with a standard `std::mutex` is relatively straightforward. But writing one *without* locks forced us to learn about Compare-And-Swap (CAS) loops, data races, and progress guarantees. We quickly realized that lock-free programming is incredibly tricky to get right.

### 2. The C++ Memory Model
Using `std::memory_order_seq_cst` (sequential consistency) everywhere is safe, but it slows everything down. The real challenge was figuring out exactly where we could safely use `acquire-release` and `relaxed` memory ordering to make the queue fast without breaking correctness.

### 3. False Sharing and Cache Lines
We learned the hard way that if the producer's index and the consumer's index sit next to each other in memory (sharing a cache line), the CPU cores will constantly invalidate each other's caches. We used `alignas` to pad our variables to the hardware cache line size, which gave us a massive performance boost.

### 4. Bounded vs. Unbounded
Fixed-size (bounded) ring buffers are much easier to manage because the memory is pre-allocated. Unbounded lock-free queues introduced us to the complexities of dynamic memory allocation and safe node reclamation while threads are actively reading them.

---

## 💻 Playing with the Code

If you'd like to try out our code or run the benchmarks, you'll need a C++23 compiler and CMake. 

### Integration
You can easily pull this into your own CMake projects:

```cmake
include(FetchContent)
FetchContent_Declare(
    tsqlib
    GIT_REPOSITORY https://github.com/badri41/ThreadSafeQueueLib.git
    GIT_TAG main
)
FetchContent_MakeAvailable(tsqlib)

target_link_libraries(your_target PRIVATE ThreadSafeQueueLib)
```

### A Quick Example

```cpp
#include <iostream>
#include <thread>
#include <tsfqueue.hpp> // Main header

int main() {
    // A queue where one thread pushes and one thread pops
    tsfqueue::lockfree_spsc_unbounded<int> queue;

    std::thread producer([&]() {
        for (int i = 0; i < 1000; ++i) {
            queue.push(i);
        }
    });

    std::thread consumer([&]() {
        int value;
        for (int i = 0; i < 1000; ++i) {
            // Keep trying to pop until we get a value
            while (!queue.try_pop(value)) {
                std::this_thread::yield(); 
            }
        }
    });

    producer.join();
    consumer.join();

    std::cout << "All done without data races!\n";
    return 0;
}
```

---

## 🧪 Building and Testing

We used **GoogleTest** and Clang's **ThreadSanitizer** to make sure our lock-free logic actually works and doesn't contain hidden data races.

```bash
mkdir build && cd build
cmake ..
cmake --build . -j4
ctest --output-on-failure -j4
```

---

## 🤝 Acknowledgements

This group project was built under the awesome mentorship and guidance of **Toshit Bhaiya** as part of the **Coding Club, IIT Guwahati**.

- **Contributors**: Badri Bishal Das, Sujal Patnaik & peers
- **Blog Write-up**: [That time I got reincarnated as a lock-free queue library implementer](https://www.badribishaldas.in/blog/threadSafeQueue/)

<div align="center">
  <i>"Debugging concurrency is not trivial. It requires stronger OS and architecture knowledge to reason cleanly about what the queue is doing under the hood."</i>
</div>
