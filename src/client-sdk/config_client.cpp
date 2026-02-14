#include "configclient/config_client.h"

#include <iostream>
#include <random>
#include <sstream>

#include "configclient/config_client_impl.h"

namespace configservice {

namespace {
// Generate random instance ID
std::string GenerateInstanceId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);

    std::ostringstream oss;
    oss << "instance-" << dis(gen);
    return oss.str();
}
}  // anonymous namespace

ConfigClient::ConfigClient(const std::string& server_address, const std::string& service_name,
                           const std::string& instance_id)
    : server_address_(server_address),
      service_name_(service_name),
      instance_id_(instance_id.empty() ? GenerateInstanceId() : instance_id) {
    impl_ = std::make_unique<ConfigClientImpl>(server_address_, service_name_, instance_id_);
}

ConfigClient::~ConfigClient() {
    Stop();
}

bool ConfigClient::Start() {
    return impl_->Start();
}

void ConfigClient::Stop() {
    impl_->Stop();
}

bool ConfigClient::IsConnected() const {
    return impl_->IsConnected();
}

void ConfigClient::OnConfigUpdate(ConfigUpdateCallback callback) {
    impl_->OnConfigUpdate(callback);
}

void ConfigClient::OnConnectionStatus(ConnectionStatusCallback callback) {
    impl_->OnConnectionStatus(callback);
}

ConfigData ConfigClient::GetCurrentConfig() const {
    return impl_->GetCurrentConfig();
}

int64_t ConfigClient::GetCurrentVersion() const {
    return impl_->GetCurrentVersion();
}

}  // namespace configservice