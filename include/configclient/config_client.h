#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "distribution.grpc.pb.h"
#include "distribution.pb.h"

namespace configservice {

// Forward declaration
class ConfigClientImpl;

// Callback type for config updates
using ConfigUpdateCallback = std::function<void(const ConfigData& config)>;

// Callback type for connection status changes
using ConnectionStatusCallback = std::function<void(bool connected)>;

/**
 * @brief Client SDK for receiving configuration updates
 * 
 * Example:
 * @code
 *   ConfigClient client("localhost:8082", "my-service");
 *   
 *   client.OnConfigUpdate([](const ConfigData& config) {
 *       std::cout << "New config: " << config.version() << std::endl;
 *   });
 *   
 *   client.Start();
 *   // ... your app runs ...
 *   client.Stop();
 * @endcode
 */
class ConfigClient {
public:
    /**
     * @brief Construct a new Config Client
     * 
     * @param server_address Distribution service address (e.g., "localhost:8082")
     * @param service_name Name of this service
     * @param instance_id Unique instance identifier (auto-generated if empty)
     */
    ConfigClient(const std::string& server_address,
                 const std::string& service_name,
                 const std::string& instance_id = "");

    ~ConfigClient();

    // Disable copy
    ConfigClient(const ConfigClient&) = delete;
    ConfigClient& operator=(const ConfigClient&) = delete;

    /**
     * @brief Start receiving configuration updates
     */
    bool Start();

    /**
     * @brief Stop receiving updates
     */
    void Stop();

    /**
     * @brief Check if connected
     */
    bool IsConnected() const;

    /**
     * @brief Register callback for config updates
     */
    void OnConfigUpdate(ConfigUpdateCallback callback);

    /**
     * @brief Register callback for connection status
     */
    void OnConnectionStatus(ConnectionStatusCallback callback);

    /**
     * @brief Get current configuration (thread-safe)
     */
    ConfigData GetCurrentConfig() const;

    /**
     * @brief Get current version
     */
    int64_t GetCurrentVersion() const;

    /**
     * @brief Get service name
     */
    const std::string& GetServiceName() const { return service_name_; }

    /**
     * @brief Get instance ID
     */
    const std::string& GetInstanceId() const { return instance_id_; }

private:
    std::string server_address_;
    std::string service_name_;
    std::string instance_id_;

    std::unique_ptr<ConfigClientImpl> impl_;
};

} // namespace configservice