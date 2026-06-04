#include <iostream>
#include <fstream>   
#include <thread>
#include <vector>
#include <atomic>
#include <string>
#include <chrono>
#include <iomanip>
#include <numeric>
#include <algorithm>

#include "tsfqueue.hpp"
#include "lockfreeMpscUnbounded/queue.hpp"
#include "blockingMpmcUnbounded/queue.hpp"
#include "lockfreeSpscBounded/queue.hpp"
#include "lockfreeSpscUnbounded/queue.hpp"

// Use 1 Million for latency to avoid exhausting memory with unbounded queues under heavy contention
constexpr size_t TOTAL_OPS = 1000000;

inline uint64_t getTimeNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

template <typename QueueType>
void producerThreadLat(QueueType* q, size_t itemsToPush, std::atomic_bool* start) {
    while (!start->load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    
    for (size_t i = 0; i < itemsToPush; ++i) {
        uint64_t now = getTimeNs(); // Record exact time right before pushing
        
        if constexpr (requires { q->push(now); }) {
            q->push(now);
        } else if constexpr (requires { q->waitAndPush(now); }) {
            q->waitAndPush(now);
        } else if constexpr (requires { q->emplace_back(now); }) {
            q->emplace_back(now);
        }
    }
}

template <typename QueueType>
void consumerThreadLat(QueueType* q, size_t itemsToPop, std::atomic_bool* start, std::vector<uint64_t>* latencies) {
    latencies->reserve(itemsToPop);
    
    while (!start->load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    
    size_t poppedCount = 0;
    uint64_t pushedTime = 0;
    
    while (poppedCount < itemsToPop) {
        bool popped = false;
        
        if constexpr (requires { q->tryPop(pushedTime); }) {
            popped = q->tryPop(pushedTime);
        } else if constexpr (requires { q->waitAndPop(pushedTime); }) {
            q->waitAndPop(pushedTime);
            popped = true;
        }
        
        if (popped) {
            uint64_t popTime = getTimeNs(); // Record exact time right after popping
            latencies->push_back(popTime - pushedTime); // Calculate transit time
            poppedCount++;
        }
       
    }
}

template <typename QueueType>
void runLatencyBenchmark(int numProducers, int numConsumers, const std::string& queueName, std::ofstream& outfile) {
    QueueType q;
    const size_t itemsPerProducer = TOTAL_OPS / numProducers;
    const size_t itemsPerConsumer = TOTAL_OPS / numConsumers;
    
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    
    // Each consumer gets its own vector to avoid locking/false-sharing
    std::vector<std::vector<uint64_t>> consumerLatencies(numConsumers);
    std::atomic_bool start{ false };

    // Spin up Consumers
    for (int i = 0; i < numConsumers; ++i) {
        consumers.emplace_back(consumerThreadLat<QueueType>, &q, itemsPerConsumer, &start, &consumerLatencies[i]);
    }
    
    // Spin up Producers
    for (int i = 0; i < numProducers; ++i) {
        producers.emplace_back(producerThreadLat<QueueType>, &q, itemsPerProducer, &start);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Unleash threads
    start.store(true, std::memory_order_release);

    for (auto& c : consumers) c.join();
    for (auto& p : producers) p.join();

    // Aggregate all latency data
    std::vector<uint64_t> allLatencies;
    allLatencies.reserve(TOTAL_OPS);
    for (const auto& vec : consumerLatencies) {
        allLatencies.insert(allLatencies.end(), vec.begin(), vec.end());
    }

    // Sort to calculate percentiles
    std::sort(allLatencies.begin(), allLatencies.end());

    // Nanoseconds to microseconds (us) for readability
    double avgUs = (std::accumulate(allLatencies.begin(), allLatencies.end(), 0.0) / allLatencies.size()) / 1000.0;
    double p50_us = allLatencies[allLatencies.size() * 0.50] / 1000.0;
    double p99_us = allLatencies[allLatencies.size() * 0.99] / 1000.0;
    double p999_us = allLatencies[allLatencies.size() * 0.999] / 1000.0;

    std::string label = "P=" + std::to_string(numProducers) + ", C=" + std::to_string(numConsumers);

    std::cout << std::left << std::setw(28) << queueName 
              << "| " << std::left << std::setw(8) << label 
              << " | Avg: " << std::setw(8) << std::fixed << std::setprecision(2) << avgUs << " us"
              << " | p50: " << std::setw(8) << p50_us << " us"
              << " | p99: " << std::setw(8) << p99_us << " us"
              << " | p99.9: " << p999_us << " us\n";

    outfile << std::left << std::setw(28) << queueName 
            << "| " << std::left << std::setw(8) << label 
            << " | Avg: " << std::setw(8) << std::fixed << std::setprecision(2) << avgUs << " us"
            << " | p50: " << std::setw(8) << p50_us << " us"
            << " | p99: " << std::setw(8) << p99_us << " us"
            << " | p99.9: " << p999_us << " us\n";
}

int main() {
    std::ofstream outfile("latencyResults.txt");
    
    std::cout << "Starting Latency Benchmarks... Please wait...\n";
    std::cout << "Results will be saved to 'latencyResults.txt'\n\n";

    std::string header = "--- LATENCY BENCHMARK (TOTAL_OPS = " + std::to_string(TOTAL_OPS) + ") ---\n";
    header += "Note: Lower is better. Measurements are in microseconds (us).\n\n";
    std::cout << header;
    outfile << header;

    // 1. spsc Queues
    runLatencyBenchmark<tsfqueue::spscUnbounded<uint64_t>>(1, 1, "SPSC_Unbounded", outfile);
    runLatencyBenchmark<tsfqueue::spscBounded<uint64_t, 65536>>(1, 1, "SPSC_Bounded_64k", outfile);
    
    std::string divider = "--------------------------------------------------------------------------------------\n";
    std::cout << divider;
    outfile << divider;

    // 2. mpsc Unbounded Queue
    for (int p : {1, 2, 4, 8}) {
        runLatencyBenchmark<tsfqueue::mpscUnbounded<uint64_t>>(p, 1, "MPSC_Unbounded", outfile);
    }
    std::cout << divider;
    outfile << divider;

    // 3. mpmc Blocking Queue
    for (int numThreads : {1, 2, 4, 8}) {
        runLatencyBenchmark<tsfqueue::blockingMpmcUnbounded<uint64_t>>(numThreads, numThreads, "MPMC_Blocking", outfile);
    }

    outfile.close();
    
    std::cout << "\nLatency benchmarks complete! Press Enter to exit...";
    std::cin.get(); 

    return 0;
}