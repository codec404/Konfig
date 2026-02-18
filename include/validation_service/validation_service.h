#pragma once

#include "config.h"

#include <grpcpp/grpcpp.h>

#include <hiredis/hiredis.h>
#include <memory>
#include <mutex>
#include <string>

#include "database_manager.h"
#include "json_validator.h"
#include "statsdclient/statsd_client.h"
#include "validation.grpc.pb.h"
#include "yaml_validator.h"

namespace validationservice {

class ValidationServiceImpl final : public configservice::ValidationService::Service {
   public:
    explicit ValidationServiceImpl(const ServiceConfig& config);
    ~ValidationServiceImpl();

    bool Initialize();
    void Shutdown();

    // ─────────────────────────────────────────────
    // gRPC Methods
    // ─────────────────────────────────────────────

    grpc::Status ValidateConfig(grpc::ServerContext* context,
                                const configservice::ValidateConfigRequest* request,
                                configservice::ValidateConfigResponse* response) override;

    grpc::Status RegisterSchema(grpc::ServerContext* context,
                                const configservice::RegisterSchemaRequest* request,
                                configservice::RegisterSchemaResponse* response) override;

    grpc::Status GetSchema(grpc::ServerContext* context,
                           const configservice::GetSchemaRequest* request,
                           configservice::GetSchemaResponse* response) override;

    grpc::Status ListSchemas(grpc::ServerContext* context,
                             const configservice::ListSchemasRequest* request,
                             configservice::ListSchemasResponse* response) override;

   private:
    ServiceConfig config_;
    std::unique_ptr<DatabaseManager> db_;
    std::unique_ptr<JsonValidator> json_validator_;
    std::unique_ptr<YamlValidator> yaml_validator_;
    std::unique_ptr<statsdclient::StatsDClient> statsd_;

    // Redis for caching validation results
    redisContext* redis_ctx_;
    std::mutex redis_mutex_;

    bool initialized_;

    // Helper methods
    bool ValidateSize(const std::string& content,
                      std::vector<configservice::ValidationError>& errors);

    bool ApplyCustomRules(const std::string& service_name, const std::string& content,
                          std::vector<configservice::ValidationError>& errors);

    std::string GetCachedValidationResult(const std::string& cache_key);
    void CacheValidationResult(const std::string& cache_key, const std::string& result);

    void RecordMetric(const std::string& metric);
    void RecordTimer(const std::string& metric, int milliseconds);

    std::string ComputeHash(const std::string& content);
};

}  // namespace validationservice