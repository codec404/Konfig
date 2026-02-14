#include "statsdclient/statsd_client.h"

#include <iostream>
#include <cstring>
#include <netdb.h>

namespace statsdclient {

StatsDClient::StatsDClient(const std::string& host, int port, const std::string& prefix)
    : host_(host),
      port_(port),
      prefix_(prefix),
      sock_(-1),
      rng_(std::random_device{}()),
      dist_(0.0f, 1.0f) {
    
    // Create UDP socket
    sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_ == -1) {
        std::cerr << "[StatsD] Failed to create UDP socket: " << strerror(errno) << std::endl;
        return;
    }

    // Resolve hostname to IP address
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int ret = getaddrinfo(host_.c_str(), nullptr, &hints, &result);
    if (ret != 0) {
        std::cerr << "[StatsD] Failed to resolve hostname: " << gai_strerror(ret) << std::endl;
        close(sock_);
        sock_ = -1;
        return;
    }

    // Setup server address
    memset(&server_addr_, 0, sizeof(server_addr_));
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(port_);
    server_addr_.sin_addr = ((struct sockaddr_in*)result->ai_addr)->sin_addr;

    freeaddrinfo(result);
}

StatsDClient::~StatsDClient() {
    if (sock_ >= 0) {
        close(sock_);
    }
}

void StatsDClient::increment(const std::string& metric, float sample_rate) {
    count(metric, 1, sample_rate);
}

void StatsDClient::decrement(const std::string& metric, float sample_rate) {
    count(metric, -1, sample_rate);
}

void StatsDClient::count(const std::string& metric, int value, float sample_rate) {
    send(metric, value, "c", sample_rate);
}

void StatsDClient::gauge(const std::string& metric, int value, float sample_rate) {
    send(metric, value, "g", sample_rate);
}

void StatsDClient::timing(const std::string& metric, int milliseconds, float sample_rate) {
    send(metric, milliseconds, "ms", sample_rate);
}

void StatsDClient::histogram(const std::string& metric, int value, float sample_rate) {
    send(metric, value, "h", sample_rate);
}

void StatsDClient::set(const std::string& metric, int value, float sample_rate) {
    send(metric, value, "s", sample_rate);
}

void StatsDClient::send(const std::string& metric, int value, 
                        const std::string& type, float sample_rate) {
    if (sock_ < 0) {
        return; // Not connected
    }

    // Check sampling
    if (!shouldSample(sample_rate)) {
        return;
    }

    // Build metric string: prefix.metric:value|type[@sample_rate]
    std::ostringstream oss;
    
    if (!prefix_.empty()) {
        oss << prefix_;
        if (prefix_.back() != '.') {
            oss << '.';
        }
    }
    
    oss << metric << ":" << value << "|" << type;
    
    if (sample_rate < 1.0f) {
        oss << "|@" << sample_rate;
    }

    std::string message = oss.str();

    // Send via UDP (fire and forget)
    ssize_t sent = sendto(sock_, message.c_str(), message.length(), 0,
                          (struct sockaddr*)&server_addr_, sizeof(server_addr_));
    
    if (sent < 0) {
        // Don't log every error to avoid spam, but could add debug logging
        // std::cerr << "[StatsD] Send failed: " << strerror(errno) << std::endl;
    }
}

bool StatsDClient::shouldSample(float sample_rate) {
    if (sample_rate >= 1.0f) {
        return true;
    }
    
    if (sample_rate <= 0.0f) {
        return false;
    }

    return dist_(rng_) <= sample_rate;
}

// StatsDTimer implementation

StatsDTimer::StatsDTimer(StatsDClient& client, const std::string& metric)
    : client_(client),
      metric_(metric),
      start_(std::chrono::steady_clock::now()) {
}

StatsDTimer::~StatsDTimer() {
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);
    client_.timing(metric_, duration.count());
}

} // namespace statsdclient