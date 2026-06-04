# Benchmarking Guide & Analysis

This document provides an in-depth look at how we benchmark the performance of the various concurrent queues in **ThreadSafeQueueLib**. While the main `README.md` provides a high-level summary, this guide explains our methodology, how to compile and run the benchmarks yourself, and how to interpret the raw results.

## Methodology

Measuring concurrent data structures requires care because the operating system scheduler, cache invalidation (false sharing), and atomic instruction overhead (like Compare-And-Swap retries) can heavily skew results. We measure two primary dimensions of performance: **Throughput** and **Latency**.

### 1. Throughput (Operations Per Second)
Throughput measures the sheer volume of data a queue can handle. 
- We spawn $P$ producer threads and $C$ consumer threads.
- Each thread is tasked with pushing or popping a specific number of items.
- We measure the total time taken from the start of the first operation to the completion of the final thread's join, and calculate `Total Operations / Time`.

### 2. Latency (Transit Time)
Latency measures the exact transit time of a single item through the queue.
- The producer pushes a high-precision `std::chrono::high_resolution_clock` timestamp into the queue.
- The consumer immediately reads the timestamp upon popping and calculates the delta (`Now - Timestamp`).
- Because average latency can be heavily distorted by OS thread preemption, we collect millions of these transit times and sort them to calculate **Percentiles (p50, p99, p99.9)**. 
- The `p99` latency tells us the worst-case scenario that 1% of the operations experience. In lock-free structures under heavy contention, `p99` will spike as threads continually fail their CAS loops and are forced to retry.

## Running the Benchmarks

The benchmark scripts are located in the `benchmarking/` directory.

### Building via CMake
If you have configured the project via CMake, the benchmarks are built automatically. Note that for accurate results, you **must** build in Release mode (`-O3`).

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j4
```

You can then run the executables directly:
```bash
./benchMpsc.exe
./benchSpsc.exe
```
*(Note: Since the refactor, the executables and source files use camelCase naming.)*

### Compiling Manually (g++)
You can also compile the individual benchmarks directly using `g++`. Ensure you pass the C++23 standard, optimization flags, and thread library:

```bash
g++ -O3 -std=c++23 -pthread -I../include benchThroughputMpsc.cpp -o benchThroughputMpsc.exe
./benchThroughputMpsc.exe
```

## Interpreting Results

When you run the benchmarks, they will output live tables to your terminal and save the raw data to text files (e.g., `benchmarkResultsMpsc.txt`, `latencyResultsMpsc.txt`) inside the `benchmarking/` folder.

### What to Look For:
1. **SPSC (Single-Producer, Single-Consumer):** Look for incredibly low p99 latency. Because there is no contention on the push or pop indices, the cache lines remain hot and uncontended.
2. **MPSC (Multi-Producer, Single-Consumer):** As you increase the number of producers (P=1 -> P=16), watch the `Throughput` drop and the `p99` latency increase. This is the physical manifestation of thread contention and CAS retries occurring at the tail of the queue.

---
*Note: The generated `.txt` files in this directory are meant for your personal analysis and are generated on the fly. They can be safely deleted or ignored from source control.*