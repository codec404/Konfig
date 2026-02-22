#include "configclient/config_client_impl.h"

#include <chrono>
#include <iostream>

namespace configservice {

ConfigClientImpl::ConfigClientImpl(const std::string& server_address,
                                   const std::string& service_name, const std::string& instance_id,
                                   const std::string& cache_dir)
    : server_address_(server_address), service_name_(service_name), instance_id_(instance_id),
      current_version_(0), running_(false), connected_(false) {
    // Create gRPC channel
    channel_ = grpc::CreateChannel(server_address_, grpc::InsecureChannelCredentials());
    stub_ = DistributionService::NewStub(channel_);

    // Initialise disk cache
    disk_cache_ = std::make_unique<DiskCache>(cache_dir);

    std::cout << "[ConfigClient] Created client for service: " << service_name_
              << " (instance: " << instance_id_ << ")" << std::endl;
}

ConfigClientImpl::~ConfigClientImpl() {
    Stop();
}

bool ConfigClientImpl::Start() {
    if (running_) {
        return false;
    }

    std::cout << "[ConfigClient] Starting client..." << std::endl;
    running_ = true;

    // Load cached config from disk before connecting — gives app an immediate value
    {
        ConfigData cached;
        if (disk_cache_->Load(service_name_, cached)) {
            std::lock_guard<std::mutex> lock(config_mutex_);
            current_config_ = cached;
            current_version_ = cached.version();
        }
    }

    // Start stream thread
    stream_thread_ = std::make_unique<std::thread>(&ConfigClientImpl::StreamLoop, this);

    return true;
}

void ConfigClientImpl::Stop() {
    if (!running_) {
        return;
    }

    std::cout << "[ConfigClient] Stopping client..." << std::endl;
    running_ = false;

    // Cancel gRPC context
    if (context_) {
        context_->TryCancel();
    }

    // Wake up thread
    shutdown_cv_.notify_all();

    // Wait for thread
    if (stream_thread_ && stream_thread_->joinable()) {
        stream_thread_->join();
    }

    SetConnectionStatus(false);
    std::cout << "[ConfigClient] Client stopped" << std::endl;
}

bool ConfigClientImpl::IsConnected() const {
    return connected_.load();
}

void ConfigClientImpl::OnConfigUpdate(ConfigUpdateCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    config_callback_ = callback;
}

void ConfigClientImpl::OnConnectionStatus(ConnectionStatusCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    connection_callback_ = callback;
}

ConfigData ConfigClientImpl::GetCurrentConfig() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return current_config_;
}

int64_t ConfigClientImpl::GetCurrentVersion() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return current_version_;
}

void ConfigClientImpl::StreamLoop() {
    while (running_) {
        try {
            std::cout << "[ConfigClient] Attempting to connect..." << std::endl;
            ConnectAndSubscribe();
        } catch (const std::exception& e) {
            std::cerr << "[ConfigClient] Error: " << e.what() << std::endl;
        }

        if (running_) {
            std::cout << "[ConfigClient] Reconnecting in " << kReconnectDelaySeconds
                      << " seconds..." << std::endl;

            std::unique_lock<std::mutex> lock(shutdown_mutex_);
            shutdown_cv_.wait_for(lock, std::chrono::seconds(kReconnectDelaySeconds));
        }
    }
}

void ConfigClientImpl::ConnectAndSubscribe() {
    // Create new context
    context_ = std::make_unique<grpc::ClientContext>();

    // Create bidirectional stream
    stream_ = stub_->Subscribe(context_.get());

    if (!stream_) {
        std::cerr << "[ConfigClient] Failed to create stream" << std::endl;
        SetConnectionStatus(false);
        return;
    }

    // Send subscribe request
    SubscribeRequest request;
    request.set_service_name(service_name_);
    request.set_instance_id(instance_id_);
    request.set_current_version(GetCurrentVersion());

    if (!stream_->Write(request)) {
        std::cerr << "[ConfigClient] Failed to send subscribe request" << std::endl;
        SetConnectionStatus(false);
        return;
    }

    SetConnectionStatus(true);
    std::cout << "[ConfigClient] Connected to " << server_address_ << std::endl;

    // Read updates
    ConfigUpdate update;
    while (running_ && stream_->Read(&update)) {
        HandleConfigUpdate(update);
    }

    // Connection lost
    SetConnectionStatus(false);

    grpc::Status status = stream_->Finish();
    if (!status.ok()) {
        std::cerr << "[ConfigClient] Stream ended: " << status.error_message() << std::endl;
    }
}

void ConfigClientImpl::HandleConfigUpdate(const ConfigUpdate& update) {
    if (!update.has_config()) {
        return;
    }

    const ConfigData& config = update.config();

    std::cout << "[ConfigClient] Received config update v" << config.version() << std::endl;

    // Update current config
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        current_config_ = config;
        current_version_ = config.version();
    }

    // Persist to disk cache
    disk_cache_->Save(config);

    // Trigger callback
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (config_callback_) {
            try {
                config_callback_(config);
            } catch (const std::exception& e) {
                std::cerr << "[ConfigClient] Callback error: " << e.what() << std::endl;
            }
        }
    }
}

void ConfigClientImpl::SetConnectionStatus(bool connected) {
    bool was_connected = connected_.exchange(connected);

    if (was_connected != connected) {
        std::cout << "[ConfigClient] Connection status: "
                  << (connected ? "CONNECTED" : "DISCONNECTED") << std::endl;

        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (connection_callback_) {
            try {
                connection_callback_(connected);
            } catch (const std::exception& e) {
                std::cerr << "[ConfigClient] Connection callback error: " << e.what() << std::endl;
            }
        }
    }
}

}  // namespace configservice