#pragma once

#include "config.h"

#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <string>
#include <vector>

#include "validation.pb.h"

namespace validationservice {

class DatabaseManager {
   public:
    explicit DatabaseManager(const PostgresConfig& config);
    ~DatabaseManager();

    bool Initialize();
    void Shutdown();

    // Schema operations
    std::pair<bool, std::string> RegisterSchema(const configservice::ValidationSchema& schema);

    configservice::ValidationSchema GetSchema(const std::string& schema_id);

    std::vector<configservice::ValidationSchema> ListSchemas(const std::string& service_name,
                                                             int limit, int offset,
                                                             int& total_count);

    // Validation history
    void RecordValidation(const std::string& service_name, const std::string& content, bool result,
                          const std::string& errors, const std::string& warnings,
                          const std::string& validated_by);

    // Validation rules
    struct ValidationRule {
        std::string rule_id;
        std::string service_name;
        std::string field_path;
        std::string rule_type;
        std::string rule_config;
        std::string error_message;
    };

    std::vector<ValidationRule> GetRulesForService(const std::string& service_name);

   private:
    PostgresConfig config_;
    std::unique_ptr<pqxx::connection> conn_;
    std::mutex mutex_;
    bool initialized_;

    std::string BuildConnectionString();
};

}  // namespace validationservice