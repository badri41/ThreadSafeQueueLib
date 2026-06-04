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

// Use 1 Million total operations
constexpr size_t TOTAL_OPS = 1000000;

inline uint64_t getTimeNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

// Synthetic workload to throttle producers. 
// This keeps the queue relatively empty to measure true uncontended transit latency (Metric B)
inline void spinWait(int cycles) {
    volatile int dummy = 0; // volatile prevents optimization
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
        // 1. Record the exact time before push. We will pass THIS timestamp into the queue.
        uint64_t startOp = getTimeNs(); 
        
        // 2. Perform the operation (Payload = startOp)
        if constexpr (requires { q->push(startOp); }) {
            q->push(startOp);
        } else if constexpr (requires { q->waitAndPush(startOp); }) {
            q->waitAndPush(startOp);
        } else if constexpr (requires { q->emplace_back(startOp); }) {
            q->emplace_back(startOp);
        }

        // 3. Measure Execution Time (Metric A - Push)
        uint64_t endOp = getTimeNs();
        pushLatencies->push_back(endOp - startOp); 
        
        // 4. Throttle slightly to pace the queue depth and prevent bloat
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
        // 1. Record time before pop attempt
        uint64_t startOp = getTimeNs();
        
        // 2. Attempt to pop (pushedTime will hold the producer's timestamp if successful)
        bool success = q->tryPop(pushedTime);
        
        // 3. Record time immediately after
        uint64_t endOp = getTimeNs();

        if (success) {
            // Metric A (Pop Latency): How long the tryPop function took to execute
            popLatencies->push_back(endOp - startOp);
            
            // Metric B (Transit Latency): Core-to-Core time (End Pop Time - Start Push Time)
            transitLatencies->push_back(endOp - pushedTime);
            
            poppedCount++;
            fails = 0;
        } else {
            fails++;
            // Increased threshold before yielding so we don't accidentally measure OS wake-up time 
            if (fails > 10000) { 
                std::this_thread::yield();
            }
        }
    }
}

// Helper function to calculate percentiles and print with dynamic scaling/units
void printStats(const std::string& metricName, const std::string& label, std::vector<uint64_t>& latencies, std::ofstream& outfile, double scaleFactor = 1.0, const std::string& unit = "ns") {
    if (latencies.empty()) return;
    std::sort(latencies.begin(), latencies.end());

    double avg = (std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size()) / scaleFactor;
    double p50 = (latencies[latencies.size() * 0.50]) / scaleFactor;
    double p99 = (latencies[latencies.size() * 0.99]) / scaleFactor;
    double p999 = (latencies[latencies.size() * 0.999]) / scaleFactor;

    std::cout << std::left << std::setw(25) << metricName 
              << "| " << std::left << std::setw(8) << label 
              << " | Avg: " << std::setw(8) << std::fixed << std::setprecision(2) << avg << " " << unit
              << " | p50: " << std::setw(8) << p50 << " " << unit
              << " | p99: " << std::setw(8) << p99 << " " << unit
              << " | p99.9: " << p999 << " " << unit << "\n";

    outfile << std::left << std::setw(25) << metricName 
            << "| " << std::left << std::setw(8) << label 
            << " | Avg: " << std::setw(8) << std::fixed << std::setprecision(2) << avg << " " << unit
            << " | p50: " << std::setw(8) << p50 << " " << unit
            << " | p99: " << std::setw(8) << p99 << " " << unit
            << " | p99.9: " << p999 << " " << unit << "\n";
}

template <typename QueueType>
void runLatencyBenchmark(int numProducers, int numConsumers, const std::string& queueName, std::ofstream& outfile) {
    QueueType q;
    const size_t itemsPerProducer = TOTAL_OPS / numProducers;
    const size_t itemsPerConsumer = TOTAL_OPS / numConsumers;
    
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    
    std::vector<std::vector<uint64_t>> producerPushLatencies(numProducers);
    
    // mpsc means only 1 consumer, but we keep it modular
    std::vector<std::vector<uint64_t>> consumerPopLatencies(numConsumers);
    std::vector<std::vector<uint64_t>> consumerTransitLatencies(numConsumers);
    
    std::atomic_bool start{ false };

    // Spin up Consumers 
    for (int i = 0; i < numConsumers; ++i) {
        consumers.emplace_back(consumerThreadLat<QueueType>, &q, itemsPerConsumer, &start, 
                               &consumerPopLatencies[i], &consumerTransitLatencies[i]);
    }
    
    // Spin up Producers
    for (int i = 0; i < numProducers; ++i) {
        producers.emplace_back(producerThreadLat<QueueType>, &q, itemsPerProducer, &start, 
                               &producerPushLatencies[i]);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Unleash threads
    start.store(true, std::memory_order_release);

    for (auto& c : consumers) c.join();
    for (auto& p : producers) p.join();

    // Aggregate Data
    std::vector<uint64_t> allPush, allPop, allTransit;
    allPush.reserve(TOTAL_OPS);
    allPop.reserve(TOTAL_OPS);
    allTransit.reserve(TOTAL_OPS);

    for (const auto& vec : producerPushLatencies) allPush.insert(allPush.end(), vec.begin(), vec.end());
    for (const auto& vec : consumerPopLatencies) allPop.insert(allPop.end(), vec.begin(), vec.end());
    for (const auto& vec : consumerTransitLatencies) allTransit.insert(allTransit.end(), vec.begin(), vec.end());

    std::string label = "P=" + std::to_string(numProducers) + ",C=" + std::to_string(numConsumers);

    // Print Metric A in nanoseconds (Scale = 1.0)
    printStats("PUSH EXEC (Metric A)", label, allPush, outfile, 1.0, "ns");
    printStats("POP EXEC  (Metric A)", label, allPop, outfile, 1.0, "ns");
    
    // Print Metric B in microseconds (Scale = 1000.0)
    printStats("TRANSIT   (Metric B)", label, allTransit, outfile, 1000.0, "us");
    
    std::cout << "--------------------------------------------------------------------------------------\n";
    outfile << "--------------------------------------------------------------------------------------\n";
}

int main() {
    std::ofstream outfile("latencyResultsMpsc.txt");
    
    std::cout << "Starting Comprehensive Latency Benchmarks (mpsc Only)...\n\n";
    
    std::string header = "--- LATENCY BENCHMARK (TOTAL_OPS = " + std::to_string(TOTAL_OPS) + ") ---\n";
    header += "Execution (Metric A) is in NANOSECONDS (ns).\n";
    header += "Transit   (Metric B) is in MICROSECONDS (us).\n";
    header += "Lower is better.\n";
    header += "--------------------------------------------------------------------------------------\n";
    
    std::cout << header << std::flush;
    outfile << header;

    // mpsc Unbounded Queue Test
    for (int p : {1, 2, 4, 8}) {
        runLatencyBenchmark<tsfqueue::mpscUnbounded<uint64_t>>(p, 1, "MPSC_Unbounded", outfile);
    }

    outfile.close();
    std::cout << "\nBenchmarks complete! Results saved to 'latencyResultsMpsc.txt'\n";

    return 0;
}