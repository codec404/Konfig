#pragma once

#include "config.h"

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

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
    grpc::ServerReaderWriter<ConfigUpdate, SubscribeRequest>* stream;
    std::chrono::steady_clock::time_point last_heartbeat;
    std::atomic<bool> active;
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

    // Helper methods
    ConfigData FetchConfig(const std::string& service_name, int64_t version);
    bool SendConfigToClient(std::shared_ptr<ClientInfo> client, const ConfigData& config);
    void RegisterClient(const std::string& key, std::shared_ptr<ClientInfo> client);
    void UnregisterClient(const std::string& key);
    size_t GetActiveClientCount();

    // Heartbeat monitoring
    void StartHeartbeatMonitor();
    void StopHeartbeatMonitor();
    void HeartbeatMonitorLoop();

    // Metrics
    void UpdateMetrics();
};

}  // namespace configservice