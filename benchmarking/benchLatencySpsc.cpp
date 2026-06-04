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
#include "lockfreeSpscUnbounded/queue.hpp" 

inline uint64_t getTimeNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

inline void spinWait(int cycles) {
    volatile int dummy = 0; 
    for (int i = 0; i < cycles; ++i) {
        dummy = i; 
    }
}

template <typename QueueType>
void producerThreadLat(QueueType* q, size_t itemsToPush, std::atomic_bool* start, std::vector<uint64_t>* pushLatencies) {
    pushLatencies->reserve(itemsToPush);
    
    while (!start->load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    
    for (size_t i = 0; i < itemsToPush; ++i) {
        uint64_t startOp = getTimeNs(); 
        
        if constexpr (requires { q->push(startOp); }) {
            q->push(startOp);
        } else if constexpr (requires { q->waitAndPush(startOp); }) {
            q->waitAndPush(startOp);
        } else if constexpr (requires { q->emplace_back(startOp); }) {
            q->emplace_back(startOp);
        }

        uint64_t endOp = getTimeNs();
        pushLatencies->push_back(endOp - startOp); 
        
        spinWait(200); 
    }
}

template <typename QueueType>
void consumerThreadLat(QueueType* q, size_t itemsToPop, std::atomic_bool* start, std::vector<uint64_t>* popLatencies, std::vector<uint64_t>* transitLatencies) {
    popLatencies->reserve(itemsToPop);
    transitLatencies->reserve(itemsToPop);
    
    while (!start->load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    
    size_t poppedCount = 0;
    uint64_t pushedTime = 0;
    size_t fails = 0;
    
    while (poppedCount < itemsToPop) {
        uint64_t startOp = getTimeNs();
        
        bool success = q->tryPop(pushedTime);
        
        uint64_t endOp = getTimeNs();

        if (success) {
            popLatencies->push_back(endOp - startOp);
            transitLatencies->push_back(endOp - pushedTime);
            poppedCount++;
            fails = 0;
        } else {
            fails++;
            if (fails > 10000) { 
                std::this_thread::yield();
            }
        }
    }
}

void printStats(const std::string& metricName, const std::string& label, std::vector<uint64_t>& latencies, std::ofstream& outfile, double scaleFactor = 1.0, const std::string& unit = "ns") {
    if (latencies.empty()) return;
    std::sort(latencies.begin(), latencies.end());

    double avg = (std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size()) / scaleFactor;
    double p50 = (latencies[latencies.size() * 0.50]) / scaleFactor;
    double p99 = (latencies[latencies.size() * 0.99]) / scaleFactor;
    double p999 = (latencies[latencies.size() * 0.999]) / scaleFactor;

    std::cout << std::left << std::setw(25) << metricName 
              << "| " << std::left << std::setw(15) << label 
              << " | Avg: " << std::setw(8) << std::fixed << std::setprecision(2) << avg << " " << unit
              << " | p50: " << std::setw(8) << p50 << " " << unit
              << " | p99: " << std::setw(8) << p99 << " " << unit
              << " | p99.9: " << p999 << " " << unit << "\n";

    outfile << std::left << std::setw(25) << metricName 
            << "| " << std::left << std::setw(15) << label 
            << " | Avg: " << std::setw(8) << std::fixed << std::setprecision(2) << avg << " " << unit
            << " | p50: " << std::setw(8) << p50 << " " << unit
            << " | p99: " << std::setw(8) << p99 << " " << unit
            << " | p99.9: " << p999 << " " << unit << "\n";
}

template <typename QueueType>
void runLatencyBenchmark(const std::string& queueName, size_t totalOps, std::ofstream& outfile) {
    QueueType q;
    
    std::vector<uint64_t> producerPushLatencies;
    std::vector<uint64_t> consumerPopLatencies;
    std::vector<uint64_t> consumerTransitLatencies;
    
    std::atomic_bool start{ false };

    std::thread consumer(consumerThreadLat<QueueType>, &q, totalOps, &start, 
                         &consumerPopLatencies, &consumerTransitLatencies);
    std::thread producer(producerThreadLat<QueueType>, &q, totalOps, &start, 
                         &producerPushLatencies);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    start.store(true, std::memory_order_release);

    consumer.join();
    producer.join();

    std::string label = "Ops=" + std::to_string(totalOps);

    // Print Metric B in microseconds (Scale = 1000.0)
    printStats("TRANSIT   (Metric B)", label, consumerTransitLatencies, outfile, 1000.0, "us");
    
    std::cout << "--------------------------------------------------------------------------------------\n";
    outfile << "--------------------------------------------------------------------------------------\n";
}

int main() {
    std::ofstream outfile("latencyResultsSpsc.txt");
    
    std::cout << "Starting SPSC Latency vs Total Ops Benchmarks...\n\n";
    
    std::string header = "--- LATENCY BENCHMARK vs TOTAL OPS ---\n";
    header += "Transit   (Metric B) is in MICROSECONDS (us).\n";
    header += "Lower is better.\n";
    header += "--------------------------------------------------------------------------------------\n";
    
    std::cout << header << std::flush;
    outfile << header;

    runLatencyBenchmark<tsfqueue::impl::lockfreeSpscUnbounded<uint64_t>>("SPSC", 100000, outfile);
    runLatencyBenchmark<tsfqueue::impl::lockfreeSpscUnbounded<uint64_t>>("SPSC", 1000000, outfile);
    runLatencyBenchmark<tsfqueue::impl::lockfreeSpscUnbounded<uint64_t>>("SPSC", 10000000, outfile);

    outfile.close();
    return 0;
}