#include "api_service/validation_client.h"

#include <iostream>

namespace apiservice {

ValidationClient::ValidationClient(const std::string& server_address)
    : server_address_(server_address), initialized_(false) {}

ValidationClient::~ValidationClient() {
    Shutdown();
}

bool ValidationClient::Initialize() {
    try {
        channel_ = grpc::CreateChannel(server_address_, grpc::InsecureChannelCredentials());
        stub_ = configservice::ValidationService::NewStub(channel_);

        std::cout << "[ValidationClient] Connected to " << server_address_ << std::endl;

        initialized_ = true;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[ValidationClient] Init failed: " << e.what() << std::endl;
        return false;
    }
}

void ValidationClient::Shutdown() {
    channel_.reset();
    stub_.reset();
    initialized_ = false;
}

configservice::ValidateConfigResponse ValidationClient::ValidateConfig(
    const std::string& service_name, const std::string& content, const std::string& format,
    bool strict) {
    configservice::ValidateConfigResponse response;

    if (!initialized_) {
        response.set_valid(false);
        response.set_message("Validation client not initialized");
        return response;
    }

    configservice::ValidateConfigRequest request;
    request.set_service_name(service_name);
    request.set_content(content);
    request.set_format(format);
    request.set_strict(strict);

    grpc::ClientContext context;
    std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::seconds(10);
    context.set_deadline(deadline);

    grpc::Status status = stub_->ValidateConfig(&context, request, &response);

    if (!status.ok()) {
        std::cerr << "[ValidationClient] gRPC error: " << status.error_message() << std::endl;
        response.set_valid(false);
        response.set_message("Validation service error: " + status.error_message());
    }

    return response;
}

}  // namespace apiservice