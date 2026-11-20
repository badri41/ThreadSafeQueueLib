<div align="center">

# ThreadSafeQueueLib

**Lock-Free Queue Library**

[![C++23](https://img.shields.io/badge/C++-23-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B23)
[![CMake](https://img.shields.io/badge/CMake-Build-success.svg)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
<!---->
<!-- Learning project by **[Badri Bishal Das](https://github.com/badri41)** , **[Sujal Patnaik](https://github.com/Sujal-Patnaik)** , **[Sudipto Ghosh](https://github.com/sudipto818)** , **[Biswabhusan Samal](https://github.com/biswa2006)** at IIT Guwahati* -->

[Writeup](https://www.badribishaldas.in/blog/threadSafeQueue/)

</div>

---

## ok whats this?

This Library provides queues with different producer, consumer settings.
feel free to use this and contribute.

---

## ok whats inside?

- **spsc** (Single-Producer Single-Consumer) - Lock-free versions (both bounded and unbounded).
- **mpsc** (Multi-Producer Single-Consumer) - Unbounded lock-free.
- **mpmc** (Multi-Producer Multi-Consumer) - Both traditional locking and lock-free implementations.

## ok how to use it?

prerequisites : C++23, CMake

### use with CMake

plug and play using CMake, or clone the repo and build it directly, it supports cross platform.

add this

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

### Example

```cpp
#include <iostream>
#include <thread>
#include <tsfqueue.hpp>
int main() {
    tsfqueue::spscUnbounded<int> queue;

    std::thread producer([&]() {
        for (int i = 0; i < 1000; ++i) {
            queue.push(i);
        }
    });

    std::thread consumer([&]() {
        int value;
        for (int i = 0; i < 1000; ++i) {
            while (!queue.tryPop(value)) {
                std::this_thread::yield(); 
            }
        }
    });

    producer.join();
    consumer.join();

    return 0;
}
```

---

## nah I don't trust you, how do I test this?

build and test using this

```bash
mkdir build && cd build
cmake ..
cmake --build . -j4
ctest --output-on-failure -j4
```

used GTest's for the testing.. (I still don't know was this a good Idea?)
---

## Benchmarking

you can benchmark the queue's yourself by the following way.

Throughput

```bash
g++ -O3 -std=c++23 -pthread -I./include benchmarking/benchThroughput.cpp -o benchThroughput.exe
.\benchThroughput.exe 
```

Latency

```bash

g++ -O3 -std=c++23 -pthread -I./include benchmarking/benchLatency.cpp -o benchLatency.exe
.\benchLatency.exe
```

### ok but how fast is this?

not dying alone by coding the entire stuff in assembly and trusting the "Zero Cost Abstraction's" the project's in C++.
now if I didn't care about the performance I would join the python dev room across the corner.
here are the results..

#### 1. spsc (Single-Producer Single-Consumer)

Because there is no contention between producers and consumers, SPSC architectures achieve high throughput. Here is a comparison against lock-free implementations against `std::mutex` + `std::queue`.

<div align="center">
  <img src="https://quickchart.io/chart?c=%7B%22type%22%3A%22bar%22%2C%22data%22%3A%7B%22labels%22%3A%5B%22Unbounded%20Lock-Free%22%2C%22Bounded%20Lock-Free%22%2C%22Mutex%20Baseline%22%5D%2C%22datasets%22%3A%5B%7B%22label%22%3A%22Throughput%20-%201KB%20Payload%20(Millions%20Ops%2Fsec)%22%2C%22data%22%3A%5B5.8%2C11.2%2C6.8%5D%2C%22backgroundColor%22%3A%22rgba(75%2C%20192%2C%20192%2C%200.8)%22%7D%5D%7D%2C%22options%22%3A%7B%22title%22%3A%7B%22display%22%3Atrue%2C%22text%22%3A%22SPSC%20Throughput%20Comparison%20(1KB%20Payload)%22%7D%2C%22scales%22%3A%7B%22yAxes%22%3A%5B%7B%22ticks%22%3A%7B%22beginAtZero%22%3Atrue%7D%7D%5D%7D%7D%7D" alt="SPSC Throughput vs Mutex" width="45%" />
  <img src="https://quickchart.io/chart?c=%7B%22type%22%3A%22bar%22%2C%22data%22%3A%7B%22labels%22%3A%5B%22p50%20(Median)%22%2C%22p99%22%2C%22p99.9%20(Worst%20Case)%22%5D%2C%22datasets%22%3A%5B%7B%22label%22%3A%22Transit%20Latency%20(%CE%BCs)%22%2C%22data%22%3A%5B0.7%2C99.2%2C196.4%5D%2C%22backgroundColor%22%3A%22rgba(153%2C%20102%2C%20255%2C%200.8)%22%7D%5D%7D%2C%22options%22%3A%7B%22title%22%3A%7B%22display%22%3Atrue%2C%22text%22%3A%22SPSC%20Transit%20Latency%20Distribution%22%7D%2C%22scales%22%3A%7B%22yAxes%22%3A%5B%7B%22ticks%22%3A%7B%22beginAtZero%22%3Atrue%7D%7D%5D%7D%7D%7D" alt="SPSC Latency Distribution Chart" width="45%" />
</div>

**ok but why Lock-Free??**

- For very small payloads (4 Bytes), a traditional `std::mutex` queue performs extremely well (up to 20M Ops/sec) due to modern OS futex optimizations and `std::deque` block allocation.
- However, as payload size increases to **1KB**, the critical section inside the mutex grows, causing the producer and consumer to lock each other out during data copies.
- In contrast, our **Bounded Lock-Free SPSC** queue shines under heavy payloads, sustaining **~11.2M Ops/sec**. By eliminating locks, the producer and consumer can copy massive 1KB payloads into the ring buffer completely independently, resulting in nearly a **2x performance increase** over the mutex baseline (6.8M Ops/sec).
- *(Note: The Unbounded Lock-Free queue drops to ~5.8M Ops/sec at 1KB payloads due to the overhead of the OS dynamically allocating `new`/`delete` nodes).*

#### 2. mpsc Unbounded (Multi-Producer Single-Consumer)

As the number of concurrent producers increases, contention on the queue's tail pointer increases, leading to more CAS (Compare-And-Swap) retries. This naturally decreases overall throughput and increases tail latency.

<div align="center">
  <img src="https://quickchart.io/chart?c=%7B%22type%22%3A%22bar%22%2C%22data%22%3A%7B%22labels%22%3A%5B%221%20Producer%22%2C%222%20Producers%22%2C%224%20Producers%22%2C%228%20Producers%22%2C%2216%20Producers%22%5D%2C%22datasets%22%3A%5B%7B%22label%22%3A%22Throughput%20(Millions%20Ops%2Fsec)%22%2C%22data%22%3A%5B10.9%2C6.62%2C5.91%2C4.63%2C3.71%5D%2C%22backgroundColor%22%3A%22rgba(54%2C%20162%2C%20235%2C%200.8)%22%7D%5D%7D%2C%22options%22%3A%7B%22title%22%3A%7B%22display%22%3Atrue%2C%22text%22%3A%22MPSC%20Unbounded%20Throughput%20vs%20Contention%22%7D%2C%22scales%22%3A%7B%22yAxes%22%3A%5B%7B%22ticks%22%3A%7B%22beginAtZero%22%3Atrue%7D%7D%5D%7D%7D%7D" alt="MPSC Throughput Chart" width="45%" />
  <img src="https://quickchart.io/chart?c=%7B%22type%22%3A%22line%22%2C%22data%22%3A%7B%22labels%22%3A%5B%221%20Producer%22%2C%222%20Producers%22%2C%224%20Producers%22%2C%228%20Producers%22%5D%2C%22datasets%22%3A%5B%7B%22label%22%3A%22p99%20Tail%20Latency%20(%CE%BCs)%22%2C%22data%22%3A%5B123.5%2C21634.4%2C65887.7%2C107720.3%5D%2C%22borderColor%22%3A%22rgba(255%2C%2099%2C%20132%2C%201)%22%2C%22backgroundColor%22%3A%22rgba(255%2C%2099%2C%20132%2C%200.2)%22%2C%22fill%22%3Atrue%7D%5D%7D%2C%22options%22%3A%7B%22title%22%3A%7B%22display%22%3Atrue%2C%22text%22%3A%22MPSC%20Unbounded%20p99%20Latency%20vs%20Contention%22%7D%2C%22scales%22%3A%7B%22yAxes%22%3A%5B%7B%22ticks%22%3A%7B%22beginAtZero%22%3Atrue%7D%7D%5D%7D%7D%7D" alt="MPSC Latency Chart" width="45%" />
</div>

**Throughput (1,000,000 Operations):**

- **1 Producer:** `1.09e+07 Ops/sec`
- **2 Producers:** `6.62e+06 Ops/sec`
- **4 Producers:** `5.91e+06 Ops/sec`
- **8 Producers:** `4.63e+06 Ops/sec`
- **16 Producers:** `3.71e+06 Ops/sec`

**Latency (Transit Time - 1,000,000 Operations):**

- **1 Producer:** p50: `0.70 us` | p99: `123.50 us`
- **2 Producers:** p50: `7207.30 us` | p99: `21634.40 us`
- **4 Producers:** p50: `39352.60 us` | p99: `65887.70 us`
- **8 Producers:** p50: `68444.10 us` | p99: `107720.30 us`

---

## Acknowledgements

- **Contributors**: Badri Bishal Das, Sujal Patnaik, Sudipto Ghosh & Biswabhusan Samal
- **Blog Write-up**: [Read Here](https://www.badribishaldas.in/blog/threadSafeQueue/)
