#include "api_service/api_service.h"

#include <ctime>
#include <functional>
#include <iostream>
#include <sstream>

namespace apiservice {

ApiServiceImpl::ApiServiceImpl(const ServiceConfig& config) : config_(config), initialized_(false) {
    std::cout << "[ApiService] Creating service..." << std::endl;
}

ApiServiceImpl::~ApiServiceImpl() {
    Shutdown();
}

bool ApiServiceImpl::Initialize() {
    std::cout << "[ApiService] Initializing..." << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;

    // StatsD
    statsd_ = std::make_unique<statsdclient::StatsDClient>(config_.statsd.host, config_.statsd.port,
                                                           config_.statsd.prefix);

    if (statsd_->isConnected()) {
        std::cout << "[ApiService] ✓ StatsD connected" << std::endl;
    } else {
        std::cerr << "[ApiService] ⚠ StatsD not available - continuing" << std::endl;
    }

    // Database
    db_ = std::make_unique<DatabaseManager>(config_.postgres);
    if (!db_->Initialize()) {
        std::cerr << "[ApiService] ✗ Database init failed" << std::endl;
        return false;
    }

    // Kafka
    try {
        std::string errstr;
        RdKafka::Conf* conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

        if (conf->set("bootstrap.servers", config_.kafka.brokers, errstr) !=
            RdKafka::Conf::CONF_OK) {
            std::cerr << "[ApiService] ⚠ Kafka config error: " << errstr << std::endl;
            delete conf;
        } else {
            kafka_producer_.reset(RdKafka::Producer::create(conf, errstr));
            delete conf;

            if (kafka_producer_) {
                std::cout << "[ApiService] ✓ Kafka producer created" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[ApiService] ⚠ Kafka init failed: " << e.what() << std::endl;
        // Non-critical, continue
    }

    // Validation client
    validation_client_ = std::make_unique<ValidationClient>("localhost:8083");
    if (validation_client_->Initialize()) {
        std::cout << "[ApiService] ✓ Validation client connected" << std::endl;
    } else {
        std::cerr << "[ApiService] ⚠ Validation client init failed - validation disabled"
                  << std::endl;
    }

    initialized_ = true;

    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "[ApiService] ✓ Initialized successfully" << std::endl;
    std::cout << std::endl;

    return true;
}

void ApiServiceImpl::Shutdown() {
    std::cout << "[ApiService] Shutting down..." << std::endl;
    if (kafka_producer_) {
        kafka_producer_->flush(5000);
        kafka_producer_.reset();
    }
    if (db_)
        db_->Shutdown();
    std::cout << "[ApiService] Shutdown complete" << std::endl;
}

grpc::Status ApiServiceImpl::UploadConfig(grpc::ServerContext* context,
                                          const configservice::UploadConfigRequest* request,
                                          configservice::UploadConfigResponse* response) {
    std::cout << "[ApiService] UploadConfig: service=" << request->service_name() << std::endl;
    RecordMetric("upload.request");

    // Validate required fields
    if (request->service_name().empty()) {
        response->set_success(false);
        response->set_message("service_name is required");
        return grpc::Status::OK;
    }
    if (request->content().empty()) {
        response->set_success(false);
        response->set_message("content is required");
        return grpc::Status::OK;
    }

    // Validate content if requested
    std::vector<std::string> errors;
    if (request->validate() || true) {  // Always validate
        if (!ValidateContent(request->format(), request->content(), errors)) {
            response->set_success(false);
            response->set_message("Validation failed");
            for (const auto& err : errors) {
                response->add_validation_errors(err);
            }
            RecordMetric("upload.validation_failed");
            return grpc::Status::OK;
        }
    }

    // Call validation service
    if (validation_client_) {
        auto val_response = validation_client_->ValidateConfig(
            request->service_name(), request->content(), request->format(), false);

        if (!val_response.valid()) {
            response->set_success(false);
            response->set_message("Validation service rejected config");

            for (const auto& err : val_response.errors()) {
                response->add_validation_errors(err.field() + ": " + err.message());
            }

            RecordMetric("upload.validation_service_failed");
            return grpc::Status::OK;
        }

        // Log warnings but proceed
        if (val_response.warnings_size() > 0) {
            std::cout << "[ApiService] Validation warnings:" << std::endl;
            for (const auto& warn : val_response.warnings()) {
                std::cout << "  - " << warn.field() << ": " << warn.message() << std::endl;
            }
        }
    }

    // Get next version
    int64_t next_version = db_->GetNextVersion(request->service_name());

    // Build config_id
    std::string config_id = GenerateConfigId(request->service_name(), next_version);

    // Build ConfigData matching proto
    configservice::ConfigData config;
    config.set_config_id(config_id);
    config.set_service_name(request->service_name());
    config.set_version(next_version);
    config.set_content(request->content());
    config.set_format(request->format().empty() ? "json" : request->format());
    config.set_content_hash(ComputeHash(request->content()));
    config.set_created_at(static_cast<int64_t>(std::time(nullptr)));
    config.set_created_by(request->created_by().empty() ? "api" : request->created_by());

    // Store in database
    auto [success, result] = db_->InsertConfig(config, request->description());

    if (!success) {
        response->set_success(false);
        response->set_message("Failed to store: " + result);
        RecordMetric("upload.db_failed");
        return grpc::Status::OK;
    }

    // Audit log
    db_->RecordAuditEvent(request->service_name(), config_id, "uploaded", request->created_by(),
                          "Version " + std::to_string(next_version));

    // Publish Kafka event
    PublishEvent("config.uploaded", request->service_name(), next_version, request->created_by());

    // Response
    response->set_success(true);
    response->set_config_id(config_id);
    response->set_version(next_version);
    response->set_message("Uploaded successfully");

    RecordMetric("upload.success");

    std::cout << "[ApiService] ✓ Uploaded: " << config_id << " v" << next_version << std::endl;

    return grpc::Status::OK;
}

grpc::Status ApiServiceImpl::GetConfig(grpc::ServerContext* context,
                                       const configservice::GetConfigRequest* request,
                                       configservice::GetConfigResponse* response) {
    std::cout << "[ApiService] GetConfig: id=" << request->config_id() << std::endl;
    RecordMetric("get.request");

    if (request->config_id().empty()) {
        response->set_success(false);
        response->set_message("config_id is required");
        return grpc::Status::OK;
    }

    try {
        auto config = db_->GetConfigById(request->config_id());

        if (config.config_id().empty()) {
            response->set_success(false);
            response->set_message("Config not found: " + request->config_id());
            RecordMetric("get.not_found");
            return grpc::Status::OK;
        }

        *response->mutable_config() = config;
        response->set_success(true);
        response->set_message("Success");
        RecordMetric("get.success");

    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_message("Internal error: " + std::string(e.what()));
        RecordMetric("get.error");
    }

    return grpc::Status::OK;
}

grpc::Status ApiServiceImpl::ListConfigs(grpc::ServerContext* context,
                                         const configservice::ListConfigsRequest* request,
                                         configservice::ListConfigsResponse* response) {
    std::cout << "[ApiService] ListConfigs: service=" << request->service_name() << std::endl;
    RecordMetric("list.request");

    try {
        int limit = request->limit() == 0 ? 50 : request->limit();
        int offset = request->offset();
        int total_count = 0;

        auto configs = db_->ListConfigs(request->service_name(), limit, offset, total_count);

        for (const auto& config : configs) {
            *response->add_configs() = config;
        }

        response->set_success(true);
        response->set_total_count(total_count);
        RecordMetric("list.success");

    } catch (const std::exception& e) {
        response->set_success(false);
        RecordMetric("list.error");
    }

    return grpc::Status::OK;
}

grpc::Status ApiServiceImpl::DeleteConfig(grpc::ServerContext* context,
                                          const configservice::DeleteConfigRequest* request,
                                          configservice::DeleteConfigResponse* response) {
    std::cout << "[ApiService] DeleteConfig: id=" << request->config_id() << std::endl;
    RecordMetric("delete.request");

    if (request->config_id().empty()) {
        response->set_success(false);
        response->set_message("config_id is required");
        return grpc::Status::OK;
    }

    auto [success, message] = db_->DeleteConfigById(request->config_id());

    if (success) {
        db_->RecordAuditEvent("", request->config_id(), "deleted", "api", "");
        PublishEvent("config.deleted", "", 0, "api");
        RecordMetric("delete.success");
    } else {
        RecordMetric("delete.failed");
    }

    response->set_success(success);
    response->set_message(message);

    return grpc::Status::OK;
}

grpc::Status ApiServiceImpl::StartRollout(grpc::ServerContext* context,
                                          const configservice::StartRolloutRequest* request,
                                          configservice::StartRolloutResponse* response) {
    std::cout << "[ApiService] StartRollout: config=" << request->config_id() << std::endl;
    RecordMetric("rollout.request");

    if (request->config_id().empty()) {
        response->set_success(false);
        response->set_message("config_id is required");
        return grpc::Status::OK;
    }

    // Verify config exists
    auto config = db_->GetConfigById(request->config_id());
    if (config.config_id().empty()) {
        response->set_success(false);
        response->set_message("Config not found: " + request->config_id());
        return grpc::Status::OK;
    }

    int32_t target_pct = request->target_percentage() == 0 ? 100 : request->target_percentage();

    auto [success, rollout_id] =
        db_->CreateRollout(request->config_id(), request->strategy(), target_pct);

    if (!success) {
        response->set_success(false);
        response->set_message("Failed to create rollout: " + rollout_id);
        RecordMetric("rollout.failed");
        return grpc::Status::OK;
    }

    // Publish rollout event
    PublishEvent("config.rollout_started", config.service_name(), config.version(), "api");

    response->set_success(true);
    response->set_rollout_id(rollout_id);
    response->set_message("Rollout started successfully");

    RecordMetric("rollout.success");

    std::cout << "[ApiService] ✓ Rollout started: " << rollout_id << std::endl;

    return grpc::Status::OK;
}

grpc::Status ApiServiceImpl::GetRolloutStatus(grpc::ServerContext* context,
                                              const configservice::GetRolloutStatusRequest* request,
                                              configservice::GetRolloutStatusResponse* response) {
    std::cout << "[ApiService] GetRolloutStatus: config=" << request->config_id() << std::endl;
    RecordMetric("rollout_status.request");

    try {
        // Get rollout state
        auto state = db_->GetRolloutState(request->config_id());
        *response->mutable_rollout_state() = state;

        // Get affected instances
        auto config = db_->GetConfigById(request->config_id());
        if (!config.service_name().empty()) {
            auto instances = db_->GetServiceInstances(config.service_name());
            for (const auto& instance : instances) {
                *response->add_instances() = instance;
            }
        }

        response->set_success(true);
        RecordMetric("rollout_status.success");

    } catch (const std::exception& e) {
        response->set_success(false);
        RecordMetric("rollout_status.error");
    }

    return grpc::Status::OK;
}

grpc::Status ApiServiceImpl::Rollback(grpc::ServerContext* context,
                                      const configservice::RollbackRequest* request,
                                      configservice::RollbackResponse* response) {
    std::cout << "[ApiService] Rollback: service=" << request->service_name()
              << " to_version=" << request->target_version() << std::endl;
    RecordMetric("rollback.request");

    if (request->service_name().empty()) {
        response->set_success(false);
        response->set_message("service_name is required");
        return grpc::Status::OK;
    }

    try {
        configservice::ConfigData target;

        // target_version 0 means previous version
        if (request->target_version() == 0) {
            // Get current version then go back one
            auto current = db_->GetLatestConfig(request->service_name());
            if (current.version() <= 1) {
                response->set_success(false);
                response->set_message("No previous version to rollback to");
                return grpc::Status::OK;
            }
            target = db_->GetConfigByVersion(request->service_name(), current.version() - 1);
        } else {
            target = db_->GetConfigByVersion(request->service_name(), request->target_version());
        }

        if (target.config_id().empty()) {
            response->set_success(false);
            response->set_message("Target version not found");
            RecordMetric("rollback.not_found");
            return grpc::Status::OK;
        }

        // Create new version with old content
        int64_t next_version = db_->GetNextVersion(request->service_name());
        std::string new_config_id = GenerateConfigId(request->service_name(), next_version);

        configservice::ConfigData rollback_config;
        rollback_config.set_config_id(new_config_id);
        rollback_config.set_service_name(target.service_name());
        rollback_config.set_version(next_version);
        rollback_config.set_content(target.content());
        rollback_config.set_format(target.format());
        rollback_config.set_content_hash(ComputeHash(target.content()));
        rollback_config.set_created_at(static_cast<int64_t>(std::time(nullptr)));
        rollback_config.set_created_by("rollback");

        auto [success, result] =
            db_->InsertConfig(rollback_config, "Rollback to v" + std::to_string(target.version()));

        if (!success) {
            response->set_success(false);
            response->set_message("Failed to create rollback config: " + result);
            RecordMetric("rollback.db_failed");
            return grpc::Status::OK;
        }

        // Audit
        db_->RecordAuditEvent(request->service_name(), new_config_id, "rollback", "api",
                              "Rolled back to v" + std::to_string(target.version()));

        // Publish event
        PublishEvent("config.rolled_back", request->service_name(), next_version, "api");

        response->set_success(true);
        response->set_config_id(new_config_id);
        response->set_message("Rolled back to v" + std::to_string(target.version()) + " as new v" +
                              std::to_string(next_version));

        RecordMetric("rollback.success");

        std::cout << "[ApiService] ✓ Rollback complete: " << new_config_id << std::endl;

    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_message("Internal error: " + std::string(e.what()));
        RecordMetric("rollback.error");
    }

    return grpc::Status::OK;
}

// ─────────────────────────────────────────────
// Private Helpers
// ─────────────────────────────────────────────

bool ApiServiceImpl::ValidateContent(const std::string& format, const std::string& content,
                                     std::vector<std::string>& errors) {
    if (content.empty()) {
        errors.push_back("Content cannot be empty");
        return false;
    }

    if (content.size() > 1024 * 1024) {
        errors.push_back("Content exceeds 1MB limit");
        return false;
    }

    if (format == "json" || format.empty()) {
        int depth = 0;
        bool in_string = false;
        bool escaped = false;

        for (size_t i = 0; i < content.size(); ++i) {
            char c = content[i];
            if (escaped) {
                escaped = false;
                continue;
            }
            if (c == '\\' && in_string) {
                escaped = true;
                continue;
            }
            if (c == '"') {
                in_string = !in_string;
                continue;
            }
            if (!in_string) {
                if (c == '{' || c == '[')
                    depth++;
                if (c == '}' || c == ']') {
                    // Check for trailing comma
                    for (size_t j = i; j > 0; --j) {
                        char p = content[j - 1];
                        if (p == ' ' || p == '\t' || p == '\n' || p == '\r')
                            continue;
                        if (p == ',') {
                            errors.push_back("Invalid JSON: trailing comma");
                            return false;
                        }
                        break;
                    }
                    depth--;
                    if (depth < 0) {
                        errors.push_back("Invalid JSON: unexpected closing bracket");
                        return false;
                    }
                }
            }
        }

        if (depth != 0) {
            errors.push_back("Invalid JSON: unclosed brackets");
            return false;
        }
    }

    return true;
}

bool ApiServiceImpl::PublishEvent(const std::string& event_type, const std::string& service_name,
                                  int64_t version, const std::string& performed_by) {
    if (!kafka_producer_) {
        return false;
    }

    std::ostringstream event;
    event << "{";
    event << "\"event_type\":\"" << event_type << "\",";
    event << "\"service_name\":\"" << service_name << "\",";
    event << "\"version\":" << version << ",";
    event << "\"performed_by\":\"" << performed_by << "\",";
    event << "\"timestamp\":" << std::time(nullptr);
    event << "}";

    std::string event_str = event.str();

    RdKafka::ErrorCode err = kafka_producer_->produce(
        config_.kafka.topic, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY,
        const_cast<char*>(event_str.c_str()), event_str.size(), nullptr, 0, 0, nullptr);

    if (err != RdKafka::ERR_NO_ERROR) {
        std::cerr << "[ApiService] Kafka error: " << RdKafka::err2str(err) << std::endl;
        return false;
    }

    kafka_producer_->poll(0);
    return true;
}

void ApiServiceImpl::RecordMetric(const std::string& metric) {
    if (statsd_ && statsd_->isConnected()) {
        statsd_->increment(metric);
    }
}

std::string ApiServiceImpl::GenerateConfigId(const std::string& service_name, int64_t version) {
    return service_name + "-v" + std::to_string(version);
}

std::string ApiServiceImpl::ComputeHash(const std::string& content) {
    // Simple hash using std::hash for now
    // In production, use SHA256 (openssl or similar)
    std::hash<std::string> hasher;
    size_t hash = hasher(content);

    std::ostringstream oss;
    oss << std::hex << hash;
    return oss.str();
}

}  // namespace apiservice