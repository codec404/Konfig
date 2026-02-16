#include "distribution_service/distribution_service.h"

#include <chrono>
#include <iostream>

namespace configservice {

namespace {
constexpr int kReconnectDelaySeconds = 5;
}

DistributionServiceImpl::DistributionServiceImpl(const ServiceConfig& config)
    : config_(config), running_(false) {
    std::cout << "[DistributionService] Creating service..." << std::endl;
}

DistributionServiceImpl::~DistributionServiceImpl() {
    Shutdown();
}

bool DistributionServiceImpl::Initialize() {
    std::cout << "[DistributionService] Initializing..." << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;

    // Initialize metrics first (so we can track initialization)
    metrics_ = std::make_unique<MetricsClient>(config_.statsd);
    if (!metrics_->Initialize()) {
        std::cerr << "[DistributionService] ✗ Metrics initialization failed" << std::endl;
        // Continue anyway - metrics are non-critical
    }

    // Initialize database
    db_ = std::make_unique<DatabaseManager>(config_.postgres);
    if (!db_->Initialize()) {
        std::cerr << "[DistributionService] ✗ Database initialization failed" << std::endl;
        return false;
    }

    // Initialize cache
    cache_ = std::make_unique<CacheManager>(config_.redis);
    if (!cache_->Initialize()) {
        std::cerr
            << "[DistributionService] ⚠ Cache initialization failed - continuing without cache"
            << std::endl;
        // Continue without cache - it's optional
    }

    // Initialize event publisher
    events_ = std::make_unique<EventPublisher>(config_.kafka);
    if (!events_->Initialize()) {
        std::cerr << "[DistributionService] ⚠ Event publisher initialization failed - continuing "
                     "without events"
                  << std::endl;
        // Continue without events - they're optional
    }

    // Start heartbeat monitor
    StartHeartbeatMonitor();

    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "[DistributionService] ✓ Service initialized successfully" << std::endl;
    std::cout << std::endl;

    return true;
}

void DistributionServiceImpl::Shutdown() {
    std::cout << "[DistributionService] Shutting down..." << std::endl;

    // Stop heartbeat monitor
    StopHeartbeatMonitor();

    // Disconnect all clients
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& pair : active_clients_) {
            pair.second->active = false;
        }
        active_clients_.clear();
    }

    // Shutdown components
    if (events_)
        events_->Shutdown();
    if (cache_)
        cache_->Shutdown();
    if (db_)
        db_->Shutdown();

    std::cout << "[DistributionService] Shutdown complete" << std::endl;
}

grpc::Status DistributionServiceImpl::Subscribe(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<ConfigUpdate, SubscribeRequest>* stream) {
    SubscribeRequest initial_request;

    // Read initial subscribe request
    if (!stream->Read(&initial_request)) {
        if (metrics_)
            metrics_->RecordConfigFailed();
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Failed to read subscribe request");
    }

    std::string service_name = initial_request.service_name();
    std::string instance_id = initial_request.instance_id();
    int64_t current_version = initial_request.current_version();

    std::cout << "[DistributionService] New subscription:" << std::endl;
    std::cout << "  Service:  " << service_name << std::endl;
    std::cout << "  Instance: " << instance_id << std::endl;
    std::cout << "  Version:  " << current_version << std::endl;

    // Create client info
    auto client = std::make_shared<ClientInfo>();
    client->service_name = service_name;
    client->instance_id = instance_id;
    client->current_version = current_version;
    client->stream = stream;
    client->last_heartbeat = std::chrono::steady_clock::now();
    client->active = true;

    // Register client
    std::string client_key = service_name + ":" + instance_id;
    RegisterClient(client_key, client);

    // Record metrics
    if (metrics_) {
        metrics_->RecordClientConnect();
        metrics_->SetActiveClients(GetActiveClientCount());
    }

    // Publish event
    if (events_) {
        events_->PublishClientConnect(service_name, instance_id);
    }

    // Update client status in database
    if (db_) {
        db_->UpdateClientStatus(service_name, instance_id, current_version, "connected");
    }

    // Fetch and send config if needed
    try {
        auto start = std::chrono::steady_clock::now();
        ConfigData config = FetchConfig(service_name, -1);  // -1 = latest
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (metrics_) {
            metrics_->RecordConfigFetchTime(duration.count());
        }

        if (config.version() > current_version) {
            if (!SendConfigToClient(client, config)) {
                UnregisterClient(client_key);
                if (metrics_)
                    metrics_->RecordConfigFailed();
                return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to send config");
            }

            // Update client status
            if (db_) {
                db_->UpdateClientStatus(service_name, instance_id, config.version(), "connected");
                db_->RecordConfigDelivery(service_name, instance_id, config.version());
            }

            // Publish event
            if (events_) {
                events_->PublishConfigUpdate(service_name, instance_id, config.version());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[DistributionService] Error fetching config: " << e.what() << std::endl;
        if (metrics_)
            metrics_->RecordConfigFailed();
    }

    // Keep connection alive - handle heartbeats
    SubscribeRequest request;
    while (client->active && stream->Read(&request)) {
        // Update last heartbeat
        client->last_heartbeat = std::chrono::steady_clock::now();

        if (metrics_) {
            metrics_->RecordHeartbeat();
        }

        // Send heartbeat ACK
        ConfigUpdate heartbeat;
        heartbeat.set_update_type(HEARTBEAT_ACK);

        if (!stream->Write(heartbeat)) {
            std::cout << "[DistributionService] Client disconnected: " << instance_id << std::endl;
            break;
        }
    }

    // Client disconnected
    UnregisterClient(client_key);

    if (metrics_) {
        metrics_->RecordClientDisconnect();
        metrics_->SetActiveClients(GetActiveClientCount());
    }

    if (events_) {
        events_->PublishClientDisconnect(service_name, instance_id);
    }

    if (db_) {
        db_->UpdateClientStatus(service_name, instance_id, client->current_version, "disconnected");
    }

    std::cout << "[DistributionService] Subscription ended: " << instance_id << std::endl;
    return grpc::Status::OK;
}

ConfigData DistributionServiceImpl::FetchConfig(const std::string& service_name, int64_t version) {
    ConfigData config;

    // Try cache first
    if (cache_) {
        auto cache_start = std::chrono::steady_clock::now();
        config = cache_->GetCachedConfig(service_name, version);
        auto cache_end = std::chrono::steady_clock::now();
        auto cache_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(cache_end - cache_start);

        if (metrics_) {
            metrics_->RecordCacheLookupTime(cache_duration.count());
        }

        if (config.version() > 0) {
            std::cout << "[DistributionService] Cache hit: " << service_name << " v"
                      << config.version() << std::endl;
            return config;
        }
    }

    // Fetch from database
    if (db_) {
        auto db_start = std::chrono::steady_clock::now();

        if (version <= 0) {
            config = db_->GetLatestConfig(service_name);
        } else {
            config = db_->GetConfigByVersion(service_name, version);
        }

        auto db_end = std::chrono::steady_clock::now();
        auto db_duration = std::chrono::duration_cast<std::chrono::milliseconds>(db_end - db_start);

        if (metrics_) {
            metrics_->RecordDatabaseQueryTime(db_duration.count());
        }

        // Cache the result
        if (cache_ && config.version() > 0) {
            cache_->CacheConfig(config);
        }
    }

    return config;
}

bool DistributionServiceImpl::SendConfigToClient(std::shared_ptr<ClientInfo> client,
                                                 const ConfigData& config) {
    if (!client || !client->active) {
        return false;
    }

    ConfigUpdate update;
    *update.mutable_config() = config;
    update.set_update_type(NEW_CONFIG);
    update.set_force_reload(config.version() > client->current_version);

    if (client->stream->Write(update)) {
        std::cout << "[DistributionService] Sent config v" << config.version() << " to "
                  << client->instance_id << std::endl;

        client->current_version = config.version();

        if (metrics_) {
            metrics_->RecordConfigSent();
        }

        return true;
    }

    if (metrics_) {
        metrics_->RecordConfigFailed();
    }

    return false;
}

void DistributionServiceImpl::RegisterClient(const std::string& key,
                                             std::shared_ptr<ClientInfo> client) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    active_clients_[key] = client;

    std::cout << "[DistributionService] Registered client: " << key << std::endl;
    std::cout << "  Total active clients: " << active_clients_.size() << std::endl;
}

void DistributionServiceImpl::UnregisterClient(const std::string& key) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    active_clients_.erase(key);

    std::cout << "[DistributionService] Unregistered client: " << key << std::endl;
    std::cout << "  Total active clients: " << active_clients_.size() << std::endl;
}

size_t DistributionServiceImpl::GetActiveClientCount() {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    return active_clients_.size();
}

void DistributionServiceImpl::StartHeartbeatMonitor() {
    running_ = true;
    heartbeat_thread_ =
        std::make_unique<std::thread>(&DistributionServiceImpl::HeartbeatMonitorLoop, this);

    std::cout << "[DistributionService] Heartbeat monitor started" << std::endl;
}

void DistributionServiceImpl::StopHeartbeatMonitor() {
    running_ = false;

    if (heartbeat_thread_ && heartbeat_thread_->joinable()) {
        heartbeat_thread_->join();
    }

    std::cout << "[DistributionService] Heartbeat monitor stopped" << std::endl;
}

void DistributionServiceImpl::HeartbeatMonitorLoop() {
    while (running_) {
        std::this_thread::sleep_for(
            std::chrono::seconds(config_.monitoring.heartbeat_interval_seconds));

        auto now = std::chrono::steady_clock::now();
        std::vector<std::string> dead_clients;

        {
            std::lock_guard<std::mutex> lock(clients_mutex_);

            for (auto& pair : active_clients_) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - pair.second->last_heartbeat);

                if (elapsed.count() > config_.monitoring.heartbeat_timeout_seconds) {
                    dead_clients.push_back(pair.first);
                    pair.second->active = false;
                }
            }

            for (const auto& key : dead_clients) {
                std::cout << "[DistributionService] Client timeout: " << key << std::endl;

                if (metrics_) {
                    metrics_->RecordHeartbeatTimeout();
                }

                active_clients_.erase(key);
            }
        }

        // Update metrics
        UpdateMetrics();
    }
}

void DistributionServiceImpl::UpdateMetrics() {
    if (!metrics_)
        return;

    size_t active_count = GetActiveClientCount();
    metrics_->SetActiveClients(active_count);

    // Update cache hit rate (simplified - would need counters for real implementation)
    // metrics_->SetCacheHitRate(0.85f);
}

}  // namespace configservice