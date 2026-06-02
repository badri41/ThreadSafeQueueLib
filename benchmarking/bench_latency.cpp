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
#include "lockfree_mpsc_unbounded/queue.hpp"
#include "blocking_mpmc_unbounded/queue.hpp"
#include "lockfree_spsc_bounded/queue.hpp"
#include "lockfree_spsc_unbounded/queue.hpp"

// Use 1 Million for latency to avoid exhausting memory with unbounded queues under heavy contention
constexpr size_t TOTAL_OPS = 1000000;

inline uint64_t get_time_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

template <typename QueueType>
void producer_thread_lat(QueueType* q, size_t items_to_push, std::atomic_bool* start) {
    while (!start->load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    
    for (size_t i = 0; i < items_to_push; ++i) {
        uint64_t now = get_time_ns(); // Record exact time right before pushing
        
        if constexpr (requires { q->push(now); }) {
            q->push(now);
        } else if constexpr (requires { q->wait_and_push(now); }) {
            q->wait_and_push(now);
        } else if constexpr (requires { q->emplace_back(now); }) {
            q->emplace_back(now);
        }
    }
}

template <typename QueueType>
void consumer_thread_lat(QueueType* q, size_t items_to_pop, std::atomic_bool* start, std::vector<uint64_t>* latencies) {
    latencies->reserve(items_to_pop);
    
    while (!start->load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    
    size_t popped_count = 0;
    uint64_t pushed_time = 0;
    
    while (popped_count < items_to_pop) {
        bool popped = false;
        
        if constexpr (requires { q->try_pop(pushed_time); }) {
            popped = q->try_pop(pushed_time);
        } else if constexpr (requires { q->wait_and_pop(pushed_time); }) {
            q->wait_and_pop(pushed_time);
            popped = true;
        }
        
        if (popped) {
            uint64_t pop_time = get_time_ns(); // Record exact time right after popping
            latencies->push_back(pop_time - pushed_time); // Calculate transit time
            popped_count++;
        }
       
    }
}

template <typename QueueType>
void run_latency_benchmark(int num_producers, int num_consumers, const std::string& queue_name, std::ofstream& outfile) {
    QueueType q;
    const size_t items_per_producer = TOTAL_OPS / num_producers;
    const size_t items_per_consumer = TOTAL_OPS / num_consumers;
    
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    
    // Each consumer gets its own vector to avoid locking/false-sharing
    std::vector<std::vector<uint64_t>> consumer_latencies(num_consumers);
    std::atomic_bool start{ false };

    // Spin up Consumers
    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back(consumer_thread_lat<QueueType>, &q, items_per_consumer, &start, &consumer_latencies[i]);
    }
    
    // Spin up Producers
    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back(producer_thread_lat<QueueType>, &q, items_per_producer, &start);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Unleash threads
    start.store(true, std::memory_order_release);

    for (auto& c : consumers) c.join();
    for (auto& p : producers) p.join();

    // Aggregate all latency data
    std::vector<uint64_t> all_latencies;
    all_latencies.reserve(TOTAL_OPS);
    for (const auto& vec : consumer_latencies) {
        all_latencies.insert(all_latencies.end(), vec.begin(), vec.end());
    }

    // Sort to calculate percentiles
    std::sort(all_latencies.begin(), all_latencies.end());

    // Nanoseconds to microseconds (us) for readability
    double avg_us = (std::accumulate(all_latencies.begin(), all_latencies.end(), 0.0) / all_latencies.size()) / 1000.0;
    double p50_us = all_latencies[all_latencies.size() * 0.50] / 1000.0;
    double p99_us = all_latencies[all_latencies.size() * 0.99] / 1000.0;
    double p999_us = all_latencies[all_latencies.size() * 0.999] / 1000.0;

    std::string label = "P=" + std::to_string(num_producers) + ", C=" + std::to_string(num_consumers);

    std::cout << std::left << std::setw(28) << queue_name 
              << "| " << std::left << std::setw(8) << label 
              << " | Avg: " << std::setw(8) << std::fixed << std::setprecision(2) << avg_us << " us"
              << " | p50: " << std::setw(8) << p50_us << " us"
              << " | p99: " << std::setw(8) << p99_us << " us"
              << " | p99.9: " << p999_us << " us\n";

    outfile << std::left << std::setw(28) << queue_name 
            << "| " << std::left << std::setw(8) << label 
            << " | Avg: " << std::setw(8) << std::fixed << std::setprecision(2) << avg_us << " us"
            << " | p50: " << std::setw(8) << p50_us << " us"
            << " | p99: " << std::setw(8) << p99_us << " us"
            << " | p99.9: " << p999_us << " us\n";
}

int main() {
    std::ofstream outfile("latency_results.txt");
    
    std::cout << "Starting Latency Benchmarks... Please wait...\n";
    std::cout << "Results will be saved to 'latency_results.txt'\n\n";

    std::string header = "--- LATENCY BENCHMARK (TOTAL_OPS = " + std::to_string(TOTAL_OPS) + ") ---\n";
    header += "Note: Lower is better. Measurements are in microseconds (us).\n\n";
    std::cout << header;
    outfile << header;

    // 1. SPSC Queues
    run_latency_benchmark<tsfqueue::SPSCUnbounded<uint64_t>>(1, 1, "SPSC_Unbounded", outfile);
    run_latency_benchmark<tsfqueue::SPSCBounded<uint64_t, 65536>>(1, 1, "SPSC_Bounded_64k", outfile);
    
    std::string divider = "--------------------------------------------------------------------------------------\n";
    std::cout << divider;
    outfile << divider;

    // 2. MPSC Unbounded Queue
    for (int p : {1, 2, 4, 8}) {
        run_latency_benchmark<tsfqueue::MPSCUnbounded<uint64_t>>(p, 1, "MPSC_Unbounded", outfile);
    }
    std::cout << divider;
    outfile << divider;

    // 3. MPMC Blocking Queue
    for (int num_threads : {1, 2, 4, 8}) {
        run_latency_benchmark<tsfqueue::BlockingMPMCUnbounded<uint64_t>>(num_threads, num_threads, "MPMC_Blocking", outfile);
    }

    outfile.close();
    
    std::cout << "\nLatency benchmarks complete! Press Enter to exit...";
    std::cin.get(); 

    return 0;
}