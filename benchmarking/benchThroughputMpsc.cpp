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

constexpr size_t TOTAL_OPS = 1000000;

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
    size_t fails = 0;
    while (poppedCount < totalItemsToPop) {
        if (q->tryPop(val)) {
            poppedCount++;
            fails = 0;
        } else {
            fails++;
            if (fails > 1000) {
                std::this_thread::yield();
            }
        }
    }
}

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

int main() {
    std::ofstream outfile("benchmarkResultsMpsc.txt");
    
    std::cout << "Starting Benchmarks (mpsc Only)... Please wait (This might take a minute)..." << std::endl;
    std::cout << "Results will be saved to 'benchmarkResultsMpsc.txt'" << std::endl << std::endl;

    std::string header = "--- THROUGHPUT BENCHMARK (TOTAL_OPS = " + std::to_string(TOTAL_OPS) + ") ---\n\n";
    std::cout << header << std::flush;
    outfile << header;
    
    std::string divider = "-------------------------------------------------------------------\n";

    // mpsc Unbounded Queue
    for (int p : {1, 2, 4, 8, 16}) {
        runThroughputBenchmark<tsfqueue::mpscUnbounded<int>>(p, "MPSC_Unbounded", outfile);
    }
    std::cout << divider;
    outfile << divider;

    outfile.close();
    
    std::cout << "\nBenchmarks complete!\n";

    return 0;
}
