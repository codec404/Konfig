#include "statsdclient/statsd_client.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>

int main() {
    std::cout << "\nStatsD Client Test" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "Sending metrics to statsd-exporter:9125" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << std::endl;

    // Use service name (not localhost)
    statsdclient::StatsDClient statsd("statsd-exporter", 9125, "test");

    // Send various metric types
    std::cout << "Sending counter metrics..." << std::endl;
    statsd.increment("requests");
    statsd.increment("requests");
    statsd.count("requests", 4);

    std::cout << "Sending gauge metrics..." << std::endl;
    statsd.gauge("temperature", 42);
    statsd.gauge("memory_usage_mb", 1024);

    std::cout << "Sending timing metrics..." << std::endl;
    statsd.timing("request_duration", 125);
    statsd.timing("processing_duration", 105);
    statsd.timing("database_query", 45);

    std::cout << "Testing RAII timer..." << std::endl;
    {
        statsdclient::StatsDTimer timer(statsd, "scoped_operation");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Simulate realistic workload
    std::cout << "\nSimulating workload for 10 seconds..." << std::endl;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> response_time(50, 300);
    std::uniform_int_distribution<> user_count(100, 200);

    for (int i = 0; i < 10; ++i) {
        std::cout << "  Tick " << (i + 1) << "/10" << std::endl;
        
        // Simulate requests
        statsd.increment("simulated_requests", 2);
        statsd.timing("simulated_response_time", response_time(gen));
        statsd.gauge("simulated_active_users", user_count(gen));
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\n✓ Test complete!" << std::endl;
    std::cout << "\nCheck metrics:" << std::endl;
    std::cout << "  curl http://statsd-exporter:9102/metrics | grep test_" << std::endl;
    std::cout << std::endl;

    return 0;
}