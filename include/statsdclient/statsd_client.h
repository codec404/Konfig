#pragma once

#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <netinet/in.h>
#include <random>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace statsdclient {

/**
 * @brief StatsD client for sending metrics
 *
 * Supports counters, gauges, timings, histograms, and sets.
 * Uses UDP for fire-and-forget metric reporting.
 *
 * Example usage:
 * @code
 *   StatsDClient statsd("localhost", 9125);
 *
 *   // Counter
 *   statsd.increment("requests.count");
 *   statsd.count("errors.count", 5);
 *
 *   // Gauge
 *   statsd.gauge("connections.active", 42);
 *
 *   // Timing
 *   statsd.timing("request.duration", 125);
 *
 *   // RAII Timer
 *   {
 *       StatsDTimer timer(statsd, "processing.time");
 *       // Your code here
 *   } // Automatically sends timing when timer goes out of scope
 * @endcode
 */
class StatsDClient {
   public:
    /**
     * @brief Construct a new StatsD Client
     *
     * @param host StatsD server hostname or IP (default: "localhost")
     * @param port StatsD server port (default: 9125)
     * @param prefix Optional prefix for all metrics (e.g., "myapp.")
     */
    explicit StatsDClient(const std::string& host = "localhost", int port = 9125,
                          const std::string& prefix = "");

    ~StatsDClient();

    // Disable copy
    StatsDClient(const StatsDClient&) = delete;
    StatsDClient& operator=(const StatsDClient&) = delete;

    /**
     * @brief Increment a counter by 1
     *
     * @param metric Metric name
     * @param sample_rate Sample rate (0.0 to 1.0, default 1.0)
     */
    void increment(const std::string& metric, float sample_rate = 1.0);

    /**
     * @brief Decrement a counter by 1
     *
     * @param metric Metric name
     * @param sample_rate Sample rate (0.0 to 1.0, default 1.0)
     */
    void decrement(const std::string& metric, float sample_rate = 1.0);

    /**
     * @brief Add arbitrary value to counter
     *
     * @param metric Metric name
     * @param value Value to add (can be negative)
     * @param sample_rate Sample rate (0.0 to 1.0, default 1.0)
     */
    void count(const std::string& metric, int value, float sample_rate = 1.0);

    /**
     * @brief Set a gauge value
     *
     * @param metric Metric name
     * @param value Gauge value
     * @param sample_rate Sample rate (0.0 to 1.0, default 1.0)
     */
    void gauge(const std::string& metric, int value, float sample_rate = 1.0);

    /**
     * @brief Record timing in milliseconds
     *
     * @param metric Metric name
     * @param milliseconds Duration in milliseconds
     * @param sample_rate Sample rate (0.0 to 1.0, default 1.0)
     */
    void timing(const std::string& metric, int milliseconds, float sample_rate = 1.0);

    /**
     * @brief Record histogram value
     *
     * @param metric Metric name
     * @param value Histogram value
     * @param sample_rate Sample rate (0.0 to 1.0, default 1.0)
     */
    void histogram(const std::string& metric, int value, float sample_rate = 1.0);

    /**
     * @brief Count unique occurrences
     *
     * @param metric Metric name
     * @param value Unique value
     * @param sample_rate Sample rate (0.0 to 1.0, default 1.0)
     */
    void set(const std::string& metric, int value, float sample_rate = 1.0);

    /**
     * @brief Check if client is connected
     *
     * @return true if UDP socket is valid
     */
    bool isConnected() const { return sock_ >= 0; }

    /**
     * @brief Get the configured prefix
     */
    const std::string& getPrefix() const { return prefix_; }

   private:
    void send(const std::string& metric, int value, const std::string& type, float sample_rate);
    bool shouldSample(float sample_rate);

    std::string host_;
    int port_;
    std::string prefix_;
    int sock_;
    struct sockaddr_in server_addr_;

    // Random number generator for sampling
    std::mt19937 rng_;
    std::uniform_real_distribution<float> dist_;
};

/**
 * @brief RAII timer for automatic timing measurements
 *
 * Automatically sends timing metric when the timer object
 * goes out of scope.
 *
 * Example:
 * @code
 *   void processRequest() {
 *       StatsDTimer timer(statsd, "request.process.duration");
 *       // Process request...
 *   } // Timer automatically sends metric here
 * @endcode
 */
class StatsDTimer {
   public:
    /**
     * @brief Construct a new StatsD Timer
     *
     * @param client StatsD client to use
     * @param metric Metric name
     */
    StatsDTimer(StatsDClient& client, const std::string& metric);

    ~StatsDTimer();

    // Disable copy
    StatsDTimer(const StatsDTimer&) = delete;
    StatsDTimer& operator=(const StatsDTimer&) = delete;

   private:
    StatsDClient& client_;
    std::string metric_;
    std::chrono::steady_clock::time_point start_;
};

}  // namespace statsdclient