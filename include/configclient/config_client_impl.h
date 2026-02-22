#pragma once

#include "config_client.h"
#include "configclient/disk_cache.h"

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "distribution.grpc.pb.h"

namespace configservice {

class ConfigClientImpl {
   public:
    ConfigClientImpl(const std::string& server_address, const std::string& service_name,
                     const std::string& instance_id, const std::string& cache_dir = "");

    ~ConfigClientImpl();

    bool Start();
    void Stop();
    bool IsConnected() const;

    void OnConfigUpdate(ConfigUpdateCallback callback);
    void OnConnectionStatus(ConnectionStatusCallback callback);

    ConfigData GetCurrentConfig() const;
    int64_t GetCurrentVersion() const;

    const std::string& GetServiceName() const { return service_name_; }
    const std::string& GetInstanceId() const { return instance_id_; }

   private:
    void StreamLoop();
    void ConnectAndSubscribe();
    void HandleConfigUpdate(const ConfigUpdate& update);
    void SetConnectionStatus(bool connected);

    std::string server_address_;
    std::string service_name_;
    std::string instance_id_;

    // gRPC
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<DistributionService::Stub> stub_;
    std::unique_ptr<grpc::ClientContext> context_;
    std::unique_ptr<grpc::ClientReaderWriter<SubscribeRequest, ConfigUpdate>> stream_;

    // Disk cache
    std::unique_ptr<DiskCache> disk_cache_;

    // Current config
    mutable std::mutex config_mutex_;
    ConfigData current_config_;
    int64_t current_version_;

    // Callbacks
    std::mutex callback_mutex_;
    ConfigUpdateCallback config_callback_;
    ConnectionStatusCallback connection_callback_;

    // Threading
    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    std::unique_ptr<std::thread> stream_thread_;

    std::mutex shutdown_mutex_;
    std::condition_variable shutdown_cv_;

    static constexpr int kReconnectDelaySeconds = 5;
};

}  // namespace configservice