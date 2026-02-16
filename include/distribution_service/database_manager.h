#pragma once

#include "config.h"
#include "config.pb.h"

#include <memory>
#include <mutex>
#include <pqxx/pqxx>

namespace configservice {

class DatabaseManager {
   public:
    explicit DatabaseManager(const PostgresConfig& config);
    ~DatabaseManager();

    bool Initialize();
    void Shutdown();

    // Config operations
    ConfigData GetLatestConfig(const std::string& service_name);
    ConfigData GetConfigByVersion(const std::string& service_name, int64_t version);
    std::vector<ConfigData> ListConfigs(const std::string& service_name, int limit);

    // Client status operations
    bool UpdateClientStatus(const std::string& service_name, const std::string& instance_id,
                            int64_t version, const std::string& status);

    bool RecordConfigDelivery(const std::string& service_name, const std::string& instance_id,
                              int64_t version);

   private:
    PostgresConfig config_;
    std::unique_ptr<pqxx::connection> conn_;
    std::mutex mutex_;
    bool initialized_;

    ConfigData ParseConfigRow(const pqxx::row& row);
    std::string BuildConnectionString();
};

}  // namespace configservice