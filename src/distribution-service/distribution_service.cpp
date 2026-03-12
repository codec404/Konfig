#include "distribution_service/distribution_service.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>

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

    // Start rollout consumer
    StartRolloutConsumer();

    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "[DistributionService] ✓ Service initialized successfully" << std::endl;
    std::cout << std::endl;

    return true;
}

void DistributionServiceImpl::Shutdown() {
    std::cout << "[DistributionService] Shutting down..." << std::endl;

    // Stop heartbeat monitor
    StopHeartbeatMonitor();

    // Stop rollout consumer
    StopRolloutConsumer();

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
    client->context = context;
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
        // Only send the latest *rolled-out* version on connect, not the latest uploaded.
        // This ensures uploads don't bypass rollout strategies.
        ConfigData config =
            db_ ? db_->GetLatestRolledOutConfig(service_name) : FetchConfig(service_name, -1);
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

        {
            std::lock_guard<std::mutex> write_lock(client->write_mutex);
            if (!stream->Write(heartbeat)) {
                std::cout << "[DistributionService] Client disconnected: " << instance_id
                          << std::endl;
                break;
            }
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

    // Fail fast if the stream is already known to be dead.
    if (client->context && client->context->IsCancelled()) {
        return false;
    }

    ConfigUpdate update;
    *update.mutable_config() = config;
    update.set_update_type(NEW_CONFIG);
    update.set_force_reload(config.version() > client->current_version);

    std::lock_guard<std::mutex> write_lock(client->write_mutex);
    if (client->stream->Write(update)) {
        std::cout << "[DistributionService] Sent config v" << config.version() << " to "
                  << client->instance_id << std::endl;

        client->current_version = config.version();

        if (metrics_) {
            metrics_->RecordConfigSent();
        }

        return true;
    }

    // Write failed — cancel the context so future calls on this stream fail instantly.
    if (client->context) {
        client->context->TryCancel();
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

                // Cancel the gRPC context so stream->Read() unblocks in Subscribe
                auto it = active_clients_.find(key);
                if (it != active_clients_.end() && it->second->context) {
                    it->second->context->TryCancel();
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
}

// ─── Rollout consumer ────────────────────────────────────────────────────────

void DistributionServiceImpl::StartRolloutConsumer() {
    std::string errstr;

    // Build broker list
    std::ostringstream brokers;
    for (size_t i = 0; i < config_.kafka.brokers.size(); ++i) {
        if (i > 0)
            brokers << ",";
        brokers << config_.kafka.brokers[i];
    }

    RdKafka::Conf* conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    conf->set("bootstrap.servers", brokers.str(), errstr);
    conf->set("group.id", "distribution-service-rollout", errstr);
    conf->set("auto.offset.reset", "latest", errstr);
    conf->set("enable.auto.commit", "true", errstr);

    rollout_consumer_.reset(RdKafka::KafkaConsumer::create(conf, errstr));
    delete conf;

    if (!rollout_consumer_) {
        std::cerr << "[DistributionService] ✗ Failed to create rollout consumer: " << errstr
                  << std::endl;
        return;
    }

    rollout_consumer_->subscribe({config_.kafka.topic});
    std::cout << "[DistributionService] ✓ Rollout consumer subscribed to topic: "
              << config_.kafka.topic << std::endl;

    rollout_thread_ =
        std::make_unique<std::thread>(&DistributionServiceImpl::RolloutConsumerLoop, this);
}

void DistributionServiceImpl::StopRolloutConsumer() {
    if (rollout_consumer_) {
        rollout_consumer_->close();
        rollout_consumer_.reset();
    }
    if (rollout_thread_ && rollout_thread_->joinable()) {
        rollout_thread_->join();
    }
    std::cout << "[DistributionService] Rollout consumer stopped" << std::endl;
}

void DistributionServiceImpl::PollPendingRollouts() {
    if (!db_)
        return;

    auto pending = db_->GetPendingRollouts();
    if (pending.empty())
        return;

    std::cout << "[DistributionService] Found " << pending.size()
              << " pending rollout(s) — executing now" << std::endl;

    for (const auto& [config_id, service_name] : pending) {
        ExecuteRollout(service_name, config_id);
    }
}

void DistributionServiceImpl::RolloutConsumerLoop() {
    // Catch up any rollouts that were IN_PROGRESS before this process started
    // (covers the Kafka rebalance timing gap and service restarts)
    PollPendingRollouts();

    constexpr int kPollIntervalMs = 30000;  // re-poll DB every 30s as safety net
    int elapsed_ms = 0;

    while (running_) {
        if (!rollout_consumer_)
            break;

        RdKafka::Message* msg = rollout_consumer_->consume(100 /*ms*/);
        elapsed_ms += 100;
        if (elapsed_ms >= kPollIntervalMs) {
            elapsed_ms = 0;
            PollPendingRollouts();
        }

        if (msg->err() == RdKafka::ERR_NO_ERROR) {
            std::string payload(static_cast<const char*>(msg->payload()), msg->len());

            std::string event_type = ExtractJsonString(payload, "event_type");
            if (event_type == "config.rollout_started" || event_type == "config.rolled_back") {
                std::string service_name = ExtractJsonString(payload, "service_name");
                int64_t version = ExtractJsonInt(payload, "version");

                if (!service_name.empty() && version > 0) {
                    // Reconstruct config_id (matches GenerateConfigId in api-service)
                    std::string config_id = service_name + "-v" + std::to_string(version);
                    std::cout << "[DistributionService] Rollout event received: " << event_type
                              << " config=" << config_id << std::endl;
                    // Run in a detached thread so the consumer loop is never blocked
                    // by a slow rollout (e.g. dead clients, slow DB writes).
                    std::thread([this, service_name, config_id]() {
                        ExecuteRollout(service_name, config_id);
                    }).detach();
                }
            }
        } else if (msg->err() != RdKafka::ERR__TIMED_OUT &&
                   msg->err() != RdKafka::ERR__PARTITION_EOF) {
            std::cerr << "[DistributionService] Kafka consume error: " << msg->errstr()
                      << std::endl;
        }

        delete msg;
    }
}

// ─── Rollout execution ────────────────────────────────────────────────────────

std::vector<std::shared_ptr<ClientInfo>> DistributionServiceImpl::GetClientsForService(
    const std::string& service_name) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    std::vector<std::shared_ptr<ClientInfo>> result;
    for (auto& [key, client] : active_clients_) {
        if (client->service_name == service_name && client->active) {
            result.push_back(client);
        }
    }
    // Sort by instance_id for deterministic canary/percentage selection
    std::sort(result.begin(), result.end(),
              [](const auto& a, const auto& b) { return a->instance_id < b->instance_id; });
    return result;
}

void DistributionServiceImpl::ExecuteRollout(const std::string& service_name,
                                             const std::string& config_id) {
    if (!db_)
        return;

    // Fetch rollout parameters
    RolloutInfo rollout = db_->GetRolloutInfo(config_id);
    if (!rollout.found) {
        // No rollout record — treat as ALL_AT_ONCE (e.g. for rollback events)
        rollout.strategy = 0;
        rollout.target_percentage = 100;
    }

    // Fetch the config to push
    ConfigData config;
    try {
        config = db_->GetConfigById(config_id);
    } catch (...) {
        std::cerr << "[DistributionService] ExecuteRollout: failed to fetch config " << config_id
                  << std::endl;
        return;
    }

    if (config.version() == 0) {
        std::cerr << "[DistributionService] ExecuteRollout: config not found: " << config_id
                  << std::endl;
        return;
    }

    auto clients = GetClientsForService(service_name);
    size_t total = clients.size();

    if (total == 0) {
        std::cout << "[DistributionService] ExecuteRollout: no connected clients for "
                  << service_name << std::endl;
        // CANARY stays IN_PROGRESS — wait for clients to connect
        // ALL_AT_ONCE / PERCENTAGE with 0 clients: complete trivially (nothing to push)
        if (rollout.strategy != 1) {
            db_->UpdateRolloutProgress(config_id, 100, "COMPLETED");
        }
        return;
    }

    size_t target_count = total;  // default: ALL_AT_ONCE

    if (rollout.strategy == 1) {
        // CANARY: push to ~10% (minimum 1 instance)
        target_count = std::max(size_t(1), total * 10 / 100);
        std::cout << "[DistributionService] CANARY rollout: pushing to " << target_count << "/"
                  << total << " instances of " << service_name << std::endl;
    } else if (rollout.strategy == 2) {
        // PERCENTAGE: push to target_percentage% of instances
        target_count = total * static_cast<size_t>(rollout.target_percentage) / 100;
        target_count = std::max(size_t(1), target_count);
        std::cout << "[DistributionService] PERCENTAGE rollout: pushing to " << target_count << "/"
                  << total << " instances (" << rollout.target_percentage << "%) of "
                  << service_name << std::endl;
    } else {
        std::cout << "[DistributionService] ALL_AT_ONCE rollout: pushing to all " << total
                  << " instances of " << service_name << std::endl;
    }

    // Push config to selected clients
    size_t pushed = 0;
    for (size_t i = 0; i < target_count && i < clients.size(); ++i) {
        // Skip clients that already have this version or newer
        if (clients[i]->current_version >= config.version()) {
            pushed++;  // count as delivered — they already have it
            continue;
        }
        if (SendConfigToClient(clients[i], config)) {
            clients[i]->current_version = config.version();
            pushed++;
            if (db_) {
                db_->UpdateClientStatus(service_name, clients[i]->instance_id, config.version(),
                                        "connected");
                db_->RecordConfigDelivery(service_name, clients[i]->instance_id, config.version());
            }
            if (events_) {
                events_->PublishConfigUpdate(service_name, clients[i]->instance_id,
                                             config.version());
            }
        }
    }

    // Update rollout progress
    int32_t current_pct = total > 0 ? static_cast<int32_t>(pushed * 100 / total) : 100;

    std::string new_status;
    if (rollout.strategy == 1) {
        // CANARY stays IN_PROGRESS — operator promotes or rolls back
        new_status = "IN_PROGRESS";
    } else if (rollout.strategy == 2 && current_pct < rollout.target_percentage) {
        new_status = "IN_PROGRESS";
    } else {
        new_status = "COMPLETED";
    }

    db_->UpdateRolloutProgress(config_id, current_pct, new_status);

    std::cout << "[DistributionService] ✓ Rollout executed: " << pushed << "/" << total
              << " instances updated (" << current_pct << "%) status=" << new_status << std::endl;
}

// ─── JSON utilities ───────────────────────────────────────────────────────────

std::string DistributionServiceImpl::ExtractJsonString(const std::string& json,
                                                       const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    auto pos = json.find(search);
    if (pos == std::string::npos)
        return "";
    pos += search.size();
    auto end = json.find('"', pos);
    if (end == std::string::npos)
        return "";
    return json.substr(pos, end - pos);
}

int64_t DistributionServiceImpl::ExtractJsonInt(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    auto pos = json.find(search);
    if (pos == std::string::npos)
        return 0;
    pos += search.size();
    try {
        return std::stoll(json.substr(pos));
    } catch (...) {
        return 0;
    }
}

}  // namespace configservice