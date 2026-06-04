#include <iostream>
#include <fstream>   
#include <thread>
#include <vector>
#include <atomic>
#include <string>
#include <chrono>
#include <iomanip>

#include "tsfqueue.hpp"
#include "lockfreeMpscUnbounded/queue.hpp"
#include "blockingMpmcUnbounded/queue.hpp"
#include "lockfreeSpscBounded/queue.hpp"
#include "lockfreeSpscUnbounded/queue.hpp"

constexpr size_t TOTAL_OPS = 10000000;

template <typename QueueType>
void producerThread(QueueType* q, size_t itemsToPush, std::atomic_bool* start) {
    while (!start->load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    for (size_t i = 0; i < itemsToPush; ++i) {
        if constexpr (requires { q->push(1); }) {
            q->push(1);
        } else if constexpr (requires { q->waitAndPush(1); }) {
            q->waitAndPush(1);
        } else if constexpr (requires { q->emplace_back(1); }) {
            q->emplace_back(1);
        }
    }
}

template <typename QueueType>
void consumerThread(QueueType* q, size_t totalItemsToPop, std::atomic_bool* start) {
    while (!start->load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    size_t poppedCount = 0;
    int val = 0;
    while (poppedCount < totalItemsToPop) {
        if constexpr (requires { q->tryPop(val); }) {
            if (q->tryPop(val)) {
                poppedCount++;
            }
        } else if constexpr (requires { q->waitAndPop(val); }) {
            q->waitAndPop(val);
            poppedCount++;
        }
    }
}

// Multi-Consumer Thread Logic

template <typename QueueType>
void mpmcConsumerThread(QueueType* q, size_t itemsToPop, std::atomic_bool* start) {
    while (!start->load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    size_t poppedCount = 0;
    int val = 0;
    while (poppedCount < itemsToPop) {
        if constexpr (requires { q->tryPop(val); }) {
            if (q->tryPop(val)) {
                poppedCount++;
            }
        } else if constexpr (requires { q->waitAndPop(val); }) {
            q->waitAndPop(val);
            poppedCount++;
        }
    }
}

// SINGLE Consumer Benchmark (For spsc and mpsc)
template <typename QueueType>
void runThroughputBenchmark(int numProducers, const std::string& queueName, std::ofstream& outfile) {
    QueueType q;
    const size_t itemsPerProducer = TOTAL_OPS / numProducers;
    std::vector<std::thread> producers;
    producers.reserve(numProducers);
    std::atomic_bool start{ false };

    std::thread consumer(consumerThread<QueueType>, &q, TOTAL_OPS, &start);
    for (int i = 0; i < numProducers; ++i) {
        producers.emplace_back(producerThread<QueueType>, &q, itemsPerProducer, &start);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto startTime = std::chrono::high_resolution_clock::now();
    start.store(true, std::memory_order_release);

    if (consumer.joinable()) consumer.join();
    for (auto& p : producers) p.join();

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = endTime - startTime;
    double opsPerSecond = TOTAL_OPS / diff.count();

    std::cout << std::left << std::setw(30) << queueName 
              << "| P=" << std::left << std::setw(8) << numProducers 
              << " | Time: " << std::setw(8) << std::fixed << std::setprecision(4) << diff.count() << " s"
              << " | Throughput: " << std::scientific << std::setprecision(2) << opsPerSecond << " Ops/sec\n";

    outfile << std::left << std::setw(30) << queueName 
            << "| P=" << std::left << std::setw(8) << numProducers 
            << " | Time: " << std::setw(8) << std::fixed << std::setprecision(4) << diff.count() << " s"
            << " | Throughput: " << std::scientific << std::setprecision(2) << opsPerSecond << " Ops/sec\n";
}

// MULTI Consumer Benchmark (For mpmc)
template <typename QueueType>
void runMpmcBenchmark(int numProducers, int numConsumers, const std::string& queueName, std::ofstream& outfile) {
    QueueType q;
    const size_t itemsPerProducer = TOTAL_OPS / numProducers;
    const size_t itemsPerConsumer = TOTAL_OPS / numConsumers;
    
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    std::atomic_bool start{ false };

    // Spin up Multiple Consumers
    for (int i = 0; i < numConsumers; ++i) {
        consumers.emplace_back(mpmcConsumerThread<QueueType>, &q, itemsPerConsumer, &start);
    }
    
    // Spin up Multiple Producers
    for (int i = 0; i < numProducers; ++i) {
        producers.emplace_back(producerThread<QueueType>, &q, itemsPerProducer, &start);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto startTime = std::chrono::high_resolution_clock::now();
    start.store(true, std::memory_order_release);

    for (auto& c : consumers) c.join();
    for (auto& p : producers) p.join();

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = endTime - startTime;
    double opsPerSecond = TOTAL_OPS / diff.count();

    std::string label = "P=" + std::to_string(numProducers) + ", C=" + std::to_string(numConsumers);

    std::cout << std::left << std::setw(30) << queueName 
              << "| " << std::left << std::setw(10) << label 
              << " | Time: " << std::setw(8) << std::fixed << std::setprecision(4) << diff.count() << " s"
              << " | Throughput: " << std::scientific << std::setprecision(2) << opsPerSecond << " Ops/sec\n";

    outfile << std::left << std::setw(30) << queueName 
            << "| " << std::left << std::setw(10) << label 
            << " | Time: " << std::setw(8) << std::fixed << std::setprecision(4) << diff.count() << " s"
            << " | Throughput: " << std::scientific << std::setprecision(2) << opsPerSecond << " Ops/sec\n";
}

int main() {
    std::ofstream outfile("benchmarkResults.txt");
    
    std::cout << "Starting Benchmarks... Please wait (This might take a minute)...\n";
    std::cout << "Results will be saved to 'benchmarkResults.txt'\n\n";

    std::string header = "--- THROUGHPUT BENCHMARK (TOTAL_OPS = " + std::to_string(TOTAL_OPS) + ") ---\n\n";
    std::cout << header;
    outfile << header;

    // 1. spsc Queues
    runThroughputBenchmark<tsfqueue::spscUnbounded<int>>(1, "SPSC_Unbounded", outfile);
    runThroughputBenchmark<tsfqueue::spscBounded<int, 65536>>(1, "SPSC_Bounded_64k", outfile);
    
    std::string divider = "-------------------------------------------------------------------\n";
    std::cout << divider;
    outfile << divider;

    // 2. mpsc Unbounded Queue
    for (int p : {1, 2, 4, 8, 16}) {
        runThroughputBenchmark<tsfqueue::mpscUnbounded<int>>(p, "MPSC_Unbounded", outfile);
    }
    std::cout << divider;
    outfile << divider;

    // 3. mpmc Blocking Queue (Symmetric Producer & Consumer Scaling)
    for (int numThreads : {1, 2, 4, 8, 16}) {
        runMpmcBenchmark<tsfqueue::blockingMpmcUnbounded<int>>(numThreads, numThreads, "MPMC_Blocking", outfile);
    }

    outfile.close();
    
    std::cout << "\nBenchmarks complete! Press Enter to exit...";
    std::cin.get(); 

    return 0;
}