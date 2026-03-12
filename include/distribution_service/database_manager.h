#pragma once

#include "config.h"
#include "config.pb.h"

#include <memory>
#include <mutex>
#include <pqxx/pqxx>

namespace configservice {

struct RolloutInfo {
    int strategy = 0;  // 0=ALL_AT_ONCE, 1=CANARY, 2=PERCENTAGE
    int32_t target_percentage = 100;
    std::string status;  // "PENDING", "IN_PROGRESS", "COMPLETED", "FAILED"
    bool found = false;
};

class DatabaseManager {
   public:
    explicit DatabaseManager(const PostgresConfig& config);
    ~DatabaseManager();

    bool Initialize();
    void Shutdown();

    // Config operations
    ConfigData GetLatestConfig(const std::string& service_name);
    ConfigData GetLatestRolledOutConfig(const std::string& service_name);
    ConfigData GetConfigByVersion(const std::string& service_name, int64_t version);
    ConfigData GetConfigById(const std::string& config_id);
    std::vector<ConfigData> ListConfigs(const std::string& service_name, int limit);

    // Client status operations
    bool UpdateClientStatus(const std::string& service_name, const std::string& instance_id,
                            int64_t version, const std::string& status);

    bool RecordConfigDelivery(const std::string& service_name, const std::string& instance_id,
                              int64_t version);

    // Rollout operations
    RolloutInfo GetRolloutInfo(const std::string& config_id);
    bool UpdateRolloutProgress(const std::string& config_id, int32_t current_pct,
                               const std::string& status);
    // Returns list of (config_id, service_name) for all IN_PROGRESS rollouts
    std::vector<std::pair<std::string, std::string>> GetPendingRollouts();

   private:
    PostgresConfig config_;
    std::unique_ptr<pqxx::connection> conn_;
    std::mutex mutex_;
    bool initialized_;

    ConfigData ParseConfigRow(const pqxx::row& row);
    std::string BuildConnectionString();
};

}  // namespace configservice