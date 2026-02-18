#pragma once

#include <grpcpp/grpcpp.h>

#include <memory>
#include <string>

#include "validation.grpc.pb.h"

namespace apiservice {

class ValidationClient {
   public:
    explicit ValidationClient(const std::string& server_address);
    ~ValidationClient();

    bool Initialize();
    void Shutdown();

    // Validate config
    configservice::ValidateConfigResponse ValidateConfig(const std::string& service_name,
                                                         const std::string& content,
                                                         const std::string& format,
                                                         bool strict = false);

   private:
    std::string server_address_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<configservice::ValidationService::Stub> stub_;
    bool initialized_;
};

}  // namespace apiservice