#pragma once

#include "config.h"

#include <grpcpp/grpcpp.h>

#include <algorithm>
#include <atomic>
#include <librdkafka/rdkafkacpp.h>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "cache_manager.h"
#include "database_manager.h"
#include "distribution.grpc.pb.h"
#include "event_publisher.h"
#include "metrics_client.h"

namespace configservice {

struct ClientInfo {
    std::string service_name;
    std::string instance_id;
    int64_t current_version;
    grpc::ServerContext* context;  // needed to cancel stream on timeout
    grpc::ServerReaderWriter<ConfigUpdate, SubscribeRequest>* stream;
    std::chrono::steady_clock::time_point last_heartbeat;
    std::atomic<bool> active;
    std::mutex write_mutex;  // serializes all stream->Write() calls
};

// Note: Class name is DistributionServiceImpl to avoid conflict with proto-generated
// DistributionService
class DistributionServiceImpl final : public DistributionService::Service {
   public:
    explicit DistributionServiceImpl(const ServiceConfig& config);
    ~DistributionServiceImpl();

    // Lifecycle
    bool Initialize();
    void Shutdown();

    // gRPC service method
    grpc::Status Subscribe(
        grpc::ServerContext* context,
        grpc::ServerReaderWriter<ConfigUpdate, SubscribeRequest>* stream) override;

   private:
    // Configuration
    ServiceConfig config_;

    // Components
    std::unique_ptr<DatabaseManager> db_;
    std::unique_ptr<CacheManager> cache_;
    std::unique_ptr<EventPublisher> events_;
    std::unique_ptr<MetricsClient> metrics_;

    // Client tracking
    std::mutex clients_mutex_;
    std::unordered_map<std::string, std::shared_ptr<ClientInfo>> active_clients_;

    // Heartbeat monitoring
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> heartbeat_thread_;

    // Rollout consumer
    std::unique_ptr<RdKafka::KafkaConsumer> rollout_consumer_;
    std::unique_ptr<std::thread> rollout_thread_;

    // Helper methods
    ConfigData FetchConfig(const std::string& service_name, int64_t version);
    bool SendConfigToClient(std::shared_ptr<ClientInfo> client, const ConfigData& config);
    void RegisterClient(const std::string& key, std::shared_ptr<ClientInfo> client);
    void UnregisterClient(const std::string& key);
    size_t GetActiveClientCount();
    std::vector<std::shared_ptr<ClientInfo>> GetClientsForService(const std::string& service_name);

    // Heartbeat monitoring
    void StartHeartbeatMonitor();
    void StopHeartbeatMonitor();
    void HeartbeatMonitorLoop();

    // Rollout consumer
    void StartRolloutConsumer();
    void StopRolloutConsumer();
    void RolloutConsumerLoop();

    // Rollout execution
    void ExecuteRollout(const std::string& service_name, const std::string& config_id);
    void PollPendingRollouts();

    // Metrics
    void UpdateMetrics();

    // Utilities
    static std::string ExtractJsonString(const std::string& json, const std::string& key);
    static int64_t ExtractJsonInt(const std::string& json, const std::string& key);
};

}  // namespace configservice