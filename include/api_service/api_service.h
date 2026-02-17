#pragma once

#include "config.h"

#include <grpcpp/grpcpp.h>

#include <librdkafka/rdkafkacpp.h>
#include <memory>
#include <string>

#include "api.grpc.pb.h"
#include "database_manager.h"
#include "statsdclient/statsd_client.h"

namespace apiservice {

class ApiServiceImpl final : public configservice::ConfigAPIService::Service {
   public:
    explicit ApiServiceImpl(const ServiceConfig& config);
    ~ApiServiceImpl();

    bool Initialize();
    void Shutdown();

    // ─────────────────────────────────────────────
    // gRPC Methods (matching proto exactly)
    // ─────────────────────────────────────────────

    grpc::Status UploadConfig(grpc::ServerContext* context,
                              const configservice::UploadConfigRequest* request,
                              configservice::UploadConfigResponse* response) override;

    grpc::Status GetConfig(grpc::ServerContext* context,
                           const configservice::GetConfigRequest* request,
                           configservice::GetConfigResponse* response) override;

    grpc::Status ListConfigs(grpc::ServerContext* context,
                             const configservice::ListConfigsRequest* request,
                             configservice::ListConfigsResponse* response) override;

    grpc::Status DeleteConfig(grpc::ServerContext* context,
                              const configservice::DeleteConfigRequest* request,
                              configservice::DeleteConfigResponse* response) override;

    grpc::Status StartRollout(grpc::ServerContext* context,
                              const configservice::StartRolloutRequest* request,
                              configservice::StartRolloutResponse* response) override;

    grpc::Status GetRolloutStatus(grpc::ServerContext* context,
                                  const configservice::GetRolloutStatusRequest* request,
                                  configservice::GetRolloutStatusResponse* response) override;

    grpc::Status Rollback(grpc::ServerContext* context,
                          const configservice::RollbackRequest* request,
                          configservice::RollbackResponse* response) override;

   private:
    ServiceConfig config_;
    std::unique_ptr<DatabaseManager> db_;
    std::unique_ptr<RdKafka::Producer> kafka_producer_;
    std::unique_ptr<statsdclient::StatsDClient> statsd_;
    bool initialized_;

    // Helpers
    bool ValidateContent(const std::string& format, const std::string& content,
                         std::vector<std::string>& errors);
    bool PublishEvent(const std::string& event_type, const std::string& service_name,
                      int64_t version, const std::string& performed_by);
    void RecordMetric(const std::string& metric);
    std::string GenerateConfigId(const std::string& service_name, int64_t version);
    std::string ComputeHash(const std::string& content);
};

}  // namespace apiservice