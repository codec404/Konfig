#pragma once

#include "config.h"

#include <memory>
#include <string>

#include "statsdclient/statsd_client.h"

namespace configservice {

class MetricsClient {
   public:
    explicit MetricsClient(const StatsDConfig& config);
    ~MetricsClient() = default;

    bool Initialize();

    // Service metrics
    void RecordClientConnect();
    void RecordClientDisconnect();
    void RecordConfigSent();
    void RecordConfigFailed();
    void RecordHeartbeat();
    void RecordHeartbeatTimeout();

    // Gauges
    void SetActiveClients(int count);
    void SetCacheHitRate(float rate);

    // Timings
    void RecordConfigFetchTime(int milliseconds);
    void RecordCacheLookupTime(int milliseconds);
    void RecordDatabaseQueryTime(int milliseconds);

   private:
    StatsDConfig config_;
    std::unique_ptr<statsdclient::StatsDClient> statsd_;
    bool initialized_;
};

}  // namespace configservice