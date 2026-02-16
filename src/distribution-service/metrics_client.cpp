#include "distribution_service/metrics_client.h"

#include <iostream>

namespace configservice {

MetricsClient::MetricsClient(const StatsDConfig& config) : config_(config), initialized_(false) {}

bool MetricsClient::Initialize() {
    try {
        statsd_ = std::make_unique<statsdclient::StatsDClient>(config_.host, config_.port,
                                                               config_.prefix);

        if (!statsd_->isConnected()) {
            std::cerr << "[Metrics] ✗ Failed to connect to StatsD" << std::endl;
            return false;
        }

        std::cout << "[Metrics] ✓ Connected to StatsD" << std::endl;
        std::cout << "[Metrics]   Host: " << config_.host << ":" << config_.port << std::endl;
        std::cout << "[Metrics]   Prefix: " << config_.prefix << std::endl;

        initialized_ = true;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[Metrics] ✗ Initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void MetricsClient::RecordClientConnect() {
    if (initialized_ && statsd_) {
        statsd_->increment("client.connect");
    }
}

void MetricsClient::RecordClientDisconnect() {
    if (initialized_ && statsd_) {
        statsd_->increment("client.disconnect");
    }
}

void MetricsClient::RecordConfigSent() {
    if (initialized_ && statsd_) {
        statsd_->increment("config.sent");
    }
}

void MetricsClient::RecordConfigFailed() {
    if (initialized_ && statsd_) {
        statsd_->increment("config.failed");
    }
}

void MetricsClient::RecordHeartbeat() {
    if (initialized_ && statsd_) {
        statsd_->increment("heartbeat.received");
    }
}

void MetricsClient::RecordHeartbeatTimeout() {
    if (initialized_ && statsd_) {
        statsd_->increment("heartbeat.timeout");
    }
}

void MetricsClient::SetActiveClients(int count) {
    if (initialized_ && statsd_) {
        statsd_->gauge("clients.active", count);
    }
}

void MetricsClient::SetCacheHitRate(float rate) {
    if (initialized_ && statsd_) {
        statsd_->gauge("cache.hit_rate", static_cast<int>(rate * 100));
    }
}

void MetricsClient::RecordConfigFetchTime(int milliseconds) {
    if (initialized_ && statsd_) {
        statsd_->timing("config.fetch_time", milliseconds);
    }
}

void MetricsClient::RecordCacheLookupTime(int milliseconds) {
    if (initialized_ && statsd_) {
        statsd_->timing("cache.lookup_time", milliseconds);
    }
}

void MetricsClient::RecordDatabaseQueryTime(int milliseconds) {
    if (initialized_ && statsd_) {
        statsd_->timing("database.query_time", milliseconds);
    }
}

}  // namespace configservice