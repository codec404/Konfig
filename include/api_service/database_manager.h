#pragma once

#include "config.h"
#include "config.pb.h"

#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <string>
#include <vector>

#include "api.pb.h"

namespace apiservice {

class DatabaseManager {
   public:
    explicit DatabaseManager(const PostgresConfig& config);
    ~DatabaseManager();

    bool Initialize();
    void Shutdown();

    // ─────────────────────────────────────────────
    // Config operations (aligned with proto)
    // ─────────────────────────────────────────────

    // Insert config - returns {success, config_id}
    std::pair<bool, std::string> InsertConfig(const configservice::ConfigData& config,
                                              const std::string& description);

    // Get by config_id (as proto defines GetConfig)
    configservice::ConfigData GetConfigById(const std::string& config_id);

    // Get latest for service (internal use)
    configservice::ConfigData GetLatestConfig(const std::string& service_name);

    // Get by version (for rollback)
    configservice::ConfigData GetConfigByVersion(const std::string& service_name, int64_t version);

    // List returns ConfigMetadata (as proto defines ListConfigs)
    std::vector<configservice::ConfigMetadata> ListConfigs(const std::string& service_name,
                                                           int limit, int offset, int& total_count);

    // Delete by config_id (as proto defines DeleteConfig)
    std::pair<bool, std::string> DeleteConfigById(const std::string& config_id);

    // ─────────────────────────────────────────────
    // Rollout operations
    // ─────────────────────────────────────────────

    std::pair<bool, std::string> CreateRollout(const std::string& config_id,
                                               configservice::RolloutStrategy strategy,
                                               int32_t target_percentage);

    configservice::RolloutState GetRolloutState(const std::string& config_id);

    std::vector<configservice::ServiceInstance> GetServiceInstances(
        const std::string& service_name);

    // ─────────────────────────────────────────────
    // Helpers
    // ─────────────────────────────────────────────

    int64_t GetNextVersion(const std::string& service_name);

    void RecordAuditEvent(const std::string& service_name, const std::string& config_id,
                          const std::string& action, const std::string& performed_by,
                          const std::string& details);

   private:
    PostgresConfig config_;
    std::unique_ptr<pqxx::connection> conn_;
    std::mutex mutex_;
    bool initialized_;

    std::string BuildConnectionString();
    configservice::ConfigData ParseConfigRow(const pqxx::row& row);
    configservice::ConfigMetadata ParseMetadataRow(const pqxx::row& row);
};

}  // namespace apiservice