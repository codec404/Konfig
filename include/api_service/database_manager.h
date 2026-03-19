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
    // Config operations
    // ─────────────────────────────────────────────

    // Insert a new version — config.config_name() and config.version() must be populated
    std::pair<bool, std::string> InsertConfig(const configservice::ConfigData& config,
                                              const std::string& description);

    // Get full config data by config_id
    configservice::ConfigData GetConfigById(const std::string& config_id);

    // Get the latest version of a named config
    configservice::ConfigData GetLatestConfigByName(const std::string& service_name,
                                                    const std::string& config_name);

    // Get a specific version of a named config (used for rollback)
    configservice::ConfigData GetConfigByVersion(const std::string& service_name,
                                                 const std::string& config_name, int64_t version);

    // Get the currently active (deployed) version of a named config
    configservice::ConfigData GetActiveConfig(const std::string& service_name,
                                              const std::string& config_name);

    // Activate one version of a named config, deactivating others in the same named config
    void SetActiveConfig(const std::string& service_name, const std::string& config_name,
                         const std::string& config_id);

    // List versions of a named config (paginated)
    std::vector<configservice::ConfigMetadata> ListConfigs(const std::string& service_name,
                                                           const std::string& config_name,
                                                           int limit, int offset, int& total_count);

    // List all named configs for a service (one summary row per config_name)
    std::vector<configservice::NamedConfigSummary> ListNamedConfigs(
        const std::string& service_name);

    // Delete a specific config version by config_id
    std::pair<bool, std::string> DeleteConfigById(const std::string& config_id);

    // ─────────────────────────────────────────────
    // Rollout operations
    // ─────────────────────────────────────────────

    std::pair<bool, std::string> CreateRollout(const std::string& config_id,
                                               configservice::RolloutStrategy strategy,
                                               int32_t target_percentage);

    configservice::RolloutState GetRolloutState(const std::string& config_id);

    std::pair<bool, std::string> PromoteRollout(const std::string& config_id,
                                                int32_t new_target_percentage);

    std::vector<configservice::ServiceInstance> GetServiceInstances(
        const std::string& service_name);

    // ─────────────────────────────────────────────
    // Helpers
    // ─────────────────────────────────────────────

    // Next version number for a named config within a service
    int64_t GetNextVersion(const std::string& service_name, const std::string& config_name);

    void RecordAuditEvent(const std::string& service_name, const std::string& config_id,
                          const std::string& action, const std::string& performed_by,
                          const std::string& details);

    // Get recent audit log entries
    std::vector<configservice::AuditEntry> GetAuditLog(const std::string& service_name, int limit);

    // Get system-wide stats
    configservice::KonfigStats GetStats();

    // List all services with summary info
    std::vector<configservice::ServiceSummary> ListServices();

    // List rollouts with optional status filter
    std::vector<configservice::RolloutSummary> ListRollouts(const std::string& status_filter,
                                                            int limit);

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