#include "validation_service/validation_service.h"

#include <chrono>
#include <climits>
#include <ctime>
#include <functional>
#include <iostream>
#include <sstream>

namespace validationservice {

ValidationServiceImpl::ValidationServiceImpl(const ServiceConfig& config)
    : config_(config), redis_ctx_(nullptr), initialized_(false) {
    std::cout << "[ValidationService] Creating service..." << std::endl;
}

ValidationServiceImpl::~ValidationServiceImpl() {
    Shutdown();
}

bool ValidationServiceImpl::Initialize() {
    std::cout << "[ValidationService] Initializing..." << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;

    // Initialize StatsD
    statsd_ = std::make_unique<statsdclient::StatsDClient>(config_.statsd.host, config_.statsd.port,
                                                           config_.statsd.prefix);

    if (statsd_->isConnected()) {
        std::cout << "[ValidationService] ✓ StatsD connected" << std::endl;
    }

    // Initialize database
    db_ = std::make_unique<DatabaseManager>(config_.postgres);
    if (!db_->Initialize()) {
        std::cerr << "[ValidationService] ✗ Database init failed" << std::endl;
        return false;
    }

    // Initialize validators
    json_validator_ = std::make_unique<JsonValidator>();
    yaml_validator_ = std::make_unique<YamlValidator>();
    std::cout << "[ValidationService] ✓ Validators initialized" << std::endl;

    // Initialize Redis for caching
    if (config_.validation.enable_caching) {
        struct timeval timeout = {5, 0};
        redis_ctx_ =
            redisConnectWithTimeout(config_.redis.host.c_str(), config_.redis.port, timeout);

        if (redis_ctx_ && !redis_ctx_->err) {
            std::cout << "[ValidationService] ✓ Redis connected (caching enabled)" << std::endl;
        } else {
            std::cerr << "[ValidationService] ⚠ Redis connection failed - caching disabled"
                      << std::endl;
            if (redis_ctx_) {
                redisFree(redis_ctx_);
                redis_ctx_ = nullptr;
            }
        }
    }

    initialized_ = true;

    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "[ValidationService] ✓ Initialized successfully" << std::endl;
    std::cout << std::endl;

    return true;
}

void ValidationServiceImpl::Shutdown() {
    std::cout << "[ValidationService] Shutting down..." << std::endl;

    if (redis_ctx_) {
        redisFree(redis_ctx_);
        redis_ctx_ = nullptr;
    }

    if (db_) {
        db_->Shutdown();
    }

    std::cout << "[ValidationService] Shutdown complete" << std::endl;
}

grpc::Status ValidationServiceImpl::ValidateConfig(
    grpc::ServerContext* context, const configservice::ValidateConfigRequest* request,
    configservice::ValidateConfigResponse* response) {
    auto start_time = std::chrono::steady_clock::now();

    std::cout << "[ValidationService] ValidateConfig: service=" << request->service_name()
              << " format=" << request->format() << std::endl;

    RecordMetric("validate.request");

    std::vector<configservice::ValidationError> errors;
    std::vector<configservice::ValidationWarning> warnings;

    // Check cache first
    std::string content_hash = ComputeHash(request->content());
    std::string cache_key = "validation:" + request->service_name() + ":" + content_hash;

    if (config_.validation.enable_caching) {
        std::string cached = GetCachedValidationResult(cache_key);
        if (!cached.empty()) {
            std::cout << "[ValidationService] Cache hit for " << cache_key << std::endl;
            RecordMetric("validate.cache_hit");

            // Parse cached result (simplified - in production, use protobuf)
            response->set_valid(cached == "valid");
            response->set_message(cached == "valid" ? "Valid (cached)" : "Invalid (cached)");
            return grpc::Status::OK;
        }
        RecordMetric("validate.cache_miss");
    }

    // 1. Validate size
    if (!ValidateSize(request->content(), errors)) {
        response->set_valid(false);
        response->set_message("Configuration exceeds maximum size");
        for (const auto& err : errors) {
            *response->add_errors() = err;
        }
        RecordMetric("validate.size_exceeded");
        return grpc::Status::OK;
    }

    // 2. Validate syntax based on format
    std::string format = request->format().empty() ? "json" : request->format();
    bool syntax_valid = false;

    if (format == "json") {
        syntax_valid = json_validator_->ValidateSyntax(request->content(), errors);
    } else if (format == "yaml" || format == "yml") {
        syntax_valid = yaml_validator_->ValidateSyntax(request->content(), errors);

        // Additional YAML structure validation
        if (syntax_valid) {
            yaml_validator_->ValidateStructure(request->content(), errors, warnings);
        }
    } else {
        configservice::ValidationError err;
        err.set_error_type("format");
        err.set_message("Unsupported format: " + format);
        errors.push_back(err);
        syntax_valid = false;
    }

    if (!syntax_valid) {
        response->set_valid(false);
        response->set_message("Syntax validation failed");
        for (const auto& err : errors) {
            *response->add_errors() = err;
        }
        RecordMetric("validate.syntax_failed");

        // Record in database
        db_->RecordValidation(request->service_name(), request->content(), false, "syntax_error",
                              "", "validation-service");

        return grpc::Status::OK;
    }

    // 3. Apply custom validation rules from database
    if (!ApplyCustomRules(request->service_name(), request->content(), errors)) {
        std::cout << "[ValidationService] Custom rule violations found" << std::endl;
        RecordMetric("validate.custom_rules_failed");
    }

    // 4. Schema validation (if schema_id provided)
    if (!request->schema_id().empty()) {
        auto schema = db_->GetSchema(request->schema_id());

        if (!schema.schema_id().empty()) {
            if (format == "json" && schema.schema_type() == "json-schema") {
                if (!json_validator_->ValidateSchema(request->content(), schema.schema_content(),
                                                     errors)) {
                    RecordMetric("validate.schema_failed");
                }
            }
        } else {
            configservice::ValidationWarning warn;
            warn.set_warning_type("schema");
            warn.set_message("Schema not found: " + request->schema_id());
            warnings.push_back(warn);
        }
    }

    // Determine final result
    bool valid = errors.empty();

    // In strict mode, warnings also cause failure
    if (request->strict() && !warnings.empty()) {
        valid = false;
        response->set_message("Validation failed in strict mode (has warnings)");
    }

    response->set_valid(valid);

    if (valid) {
        response->set_message("Configuration is valid");
        RecordMetric("validate.success");
    } else {
        response->set_message("Validation failed");
        RecordMetric("validate.failed");
    }

    // Add errors and warnings to response
    for (const auto& err : errors) {
        *response->add_errors() = err;
    }
    for (const auto& warn : warnings) {
        *response->add_warnings() = warn;
    }

    // Cache result
    if (config_.validation.enable_caching) {
        CacheValidationResult(cache_key, valid ? "valid" : "invalid");
    }

    // Record validation in database
    std::ostringstream errors_json, warnings_json;
    errors_json << "[";
    for (size_t i = 0; i < errors.size(); ++i) {
        if (i > 0)
            errors_json << ",";
        errors_json << "{\"field\":\"" << errors[i].field() << "\",\"type\":\""
                    << errors[i].error_type() << "\",\"message\":\"" << errors[i].message()
                    << "\"}";
    }
    errors_json << "]";

    warnings_json << "[";
    for (size_t i = 0; i < warnings.size(); ++i) {
        if (i > 0)
            warnings_json << ",";
        warnings_json << "{\"field\":\"" << warnings[i].field() << "\",\"type\":\""
                      << warnings[i].warning_type() << "\",\"message\":\"" << warnings[i].message()
                      << "\"}";
    }
    warnings_json << "]";

    db_->RecordValidation(request->service_name(), request->content(), valid, errors_json.str(),
                          warnings_json.str(), "validation-service");

    // Record timing
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    RecordTimer("validate.duration", duration.count());

    std::cout << "[ValidationService] Validation result: " << (valid ? "VALID" : "INVALID")
              << " (errors: " << errors.size() << ", warnings: " << warnings.size() << ")"
              << std::endl;

    return grpc::Status::OK;
}

grpc::Status ValidationServiceImpl::RegisterSchema(
    grpc::ServerContext* context, const configservice::RegisterSchemaRequest* request,
    configservice::RegisterSchemaResponse* response) {
    std::cout << "[ValidationService] RegisterSchema: id=" << request->schema_id() << std::endl;
    RecordMetric("schema.register");

    if (request->schema_id().empty()) {
        response->set_success(false);
        response->set_message("schema_id is required");
        return grpc::Status::OK;
    }

    configservice::ValidationSchema schema;
    schema.set_schema_id(request->schema_id());
    schema.set_service_name(request->service_name());
    schema.set_schema_type(request->schema_type());
    schema.set_schema_content(request->schema_content());
    schema.set_description(request->description());
    schema.set_created_by(request->created_by());
    schema.set_created_at(std::time(nullptr));
    schema.set_is_active(true);

    auto [success, message] = db_->RegisterSchema(schema);

    response->set_success(success);
    response->set_message(message);
    if (success) {
        response->set_schema_id(request->schema_id());
        RecordMetric("schema.register_success");
    } else {
        RecordMetric("schema.register_failed");
    }

    return grpc::Status::OK;
}

grpc::Status ValidationServiceImpl::GetSchema(grpc::ServerContext* context,
                                              const configservice::GetSchemaRequest* request,
                                              configservice::GetSchemaResponse* response) {
    std::cout << "[ValidationService] GetSchema: id=" << request->schema_id() << std::endl;
    RecordMetric("schema.get");

    auto schema = db_->GetSchema(request->schema_id());

    if (schema.schema_id().empty()) {
        response->set_success(false);
        response->set_message("Schema not found: " + request->schema_id());
        RecordMetric("schema.not_found");
    } else {
        response->set_success(true);
        *response->mutable_schema() = schema;
        RecordMetric("schema.get_success");
    }

    return grpc::Status::OK;
}

grpc::Status ValidationServiceImpl::ListSchemas(grpc::ServerContext* context,
                                                const configservice::ListSchemasRequest* request,
                                                configservice::ListSchemasResponse* response) {
    std::cout << "[ValidationService] ListSchemas" << std::endl;
    RecordMetric("schema.list");

    int limit = request->limit() == 0 ? 50 : request->limit();
    int offset = request->offset();
    int total_count = 0;

    auto schemas = db_->ListSchemas(request->service_name(), limit, offset, total_count);

    for (const auto& schema : schemas) {
        *response->add_schemas() = schema;
    }

    response->set_total_count(total_count);
    RecordMetric("schema.list_success");

    return grpc::Status::OK;
}

// ─────────────────────────────────────────────
// Helper Methods
// ─────────────────────────────────────────────

bool ValidationServiceImpl::ValidateSize(const std::string& content,
                                         std::vector<configservice::ValidationError>& errors) {
    if (content.size() > config_.validation.max_config_size) {
        configservice::ValidationError err;
        err.set_error_type("size");
        err.set_message("Configuration size " + std::to_string(content.size()) +
                        " bytes exceeds maximum " +
                        std::to_string(config_.validation.max_config_size) + " bytes");
        errors.push_back(err);
        return false;
    }
    return true;
}

bool ValidationServiceImpl::ApplyCustomRules(const std::string& service_name,
                                             const std::string& content,
                                             std::vector<configservice::ValidationError>& errors) {
    auto rules = db_->GetRulesForService(service_name);

    if (rules.empty()) {
        return true;  // No custom rules defined
    }

    std::cout << "[ValidationService] Applying " << rules.size() << " custom rules for "
              << service_name << std::endl;

    bool all_passed = true;

    // Helper: find a key in content (handles both JSON "key" and YAML key:)
    auto findKey = [](const std::string& content, const std::string& key, size_t from) -> size_t {
        // Try JSON format: "key"
        std::string json_search = "\"" + key + "\"";
        auto pos = content.find(json_search, from);
        if (pos != std::string::npos) {
            return pos;
        }
        // Try YAML format: key: (at start of line or after whitespace)
        std::string yaml_key = key + ":";
        pos = from;
        while (pos < content.size()) {
            pos = content.find(yaml_key, pos);
            if (pos == std::string::npos) {
                break;
            }
            // Verify it's at line start or preceded by whitespace
            if (pos == 0 || content[pos - 1] == '\n' || content[pos - 1] == ' ' ||
                content[pos - 1] == '\t') {
                return pos;
            }
            pos += yaml_key.size();
        }
        return std::string::npos;
    };

    for (const auto& rule : rules) {
        // Apply rule based on type
        if (rule.rule_type == "required") {
            // Check if field exists by walking dotted path
            // For "database.host", verify "database" contains "host"
            bool found = true;
            std::string remaining = rule.field_path;
            size_t search_from = 0;

            while (!remaining.empty()) {
                std::string key;
                auto dot = remaining.find('.');
                if (dot != std::string::npos) {
                    key = remaining.substr(0, dot);
                    remaining = remaining.substr(dot + 1);
                } else {
                    key = remaining;
                    remaining.clear();
                }

                auto pos = findKey(content, key, search_from);
                if (pos == std::string::npos) {
                    found = false;
                    break;
                }
                search_from = pos + key.size();
            }

            if (!found) {
                configservice::ValidationError err;
                err.set_field(rule.field_path);
                err.set_error_type("required");
                err.set_message(rule.error_message);
                errors.push_back(err);
                all_passed = false;
            }
        } else if (rule.rule_type == "range") {
            // Parse rule_config to get min/max
            // Simplified - in production, use JSON parser
            // Example rule_config: {"min": 1, "max": 1000}

            // Extract leaf key from dotted path
            std::string leaf_key = rule.field_path;
            auto last_dot = leaf_key.rfind('.');
            if (last_dot != std::string::npos) {
                leaf_key = leaf_key.substr(last_dot + 1);
            }

            size_t pos = findKey(content, leaf_key, 0);
            if (pos != std::string::npos) {
                size_t colon = content.find(":", pos);
                // End delimiter: comma, brace (JSON) or newline (YAML)
                size_t end = content.find_first_of(",}\n\r", colon + 1);

                if (colon != std::string::npos && end != std::string::npos) {
                    std::string value_str = content.substr(colon + 1, end - colon - 1);
                    value_str.erase(0, value_str.find_first_not_of(" \t"));
                    value_str.erase(value_str.find_last_not_of(" \t") + 1);

                    // Parse min/max from rule_config
                    int min_val = 0, max_val = INT_MAX;
                    auto min_pos = rule.rule_config.find("\"min\"");
                    auto max_pos = rule.rule_config.find("\"max\"");
                    if (min_pos != std::string::npos) {
                        auto c = rule.rule_config.find(":", min_pos);
                        if (c != std::string::npos) {
                            try {
                                min_val = std::stoi(rule.rule_config.substr(c + 1));
                            } catch (...) {
                            }
                        }
                    }
                    if (max_pos != std::string::npos) {
                        auto c = rule.rule_config.find(":", max_pos);
                        if (c != std::string::npos) {
                            try {
                                max_val = std::stoi(rule.rule_config.substr(c + 1));
                            } catch (...) {
                            }
                        }
                    }

                    try {
                        int value = std::stoi(value_str);
                        if (value < min_val || value > max_val) {
                            configservice::ValidationError err;
                            err.set_field(rule.field_path);
                            err.set_error_type("range");
                            err.set_message(rule.error_message);
                            errors.push_back(err);
                            all_passed = false;
                        }
                    } catch (...) {
                        // Invalid number
                    }
                }
            }
        }
    }

    return all_passed;
}

std::string ValidationServiceImpl::GetCachedValidationResult(const std::string& cache_key) {
    if (!redis_ctx_) {
        return "";
    }

    std::lock_guard<std::mutex> lock(redis_mutex_);

    redisReply* reply = (redisReply*)redisCommand(redis_ctx_, "GET %s", cache_key.c_str());

    if (!reply || reply->type != REDIS_REPLY_STRING) {
        if (reply)
            freeReplyObject(reply);
        return "";
    }

    std::string result(reply->str, reply->len);
    freeReplyObject(reply);

    return result;
}

void ValidationServiceImpl::CacheValidationResult(const std::string& cache_key,
                                                  const std::string& result) {
    if (!redis_ctx_) {
        return;
    }

    std::lock_guard<std::mutex> lock(redis_mutex_);

    redisReply* reply = (redisReply*)redisCommand(redis_ctx_, "SETEX %s %d %s", cache_key.c_str(),
                                                  config_.redis.cache_ttl_seconds, result.c_str());
    if (reply) {
        freeReplyObject(reply);
    }
}

void ValidationServiceImpl::RecordMetric(const std::string& metric) {
    if (statsd_ && statsd_->isConnected()) {
        statsd_->increment(metric);
    }
}

void ValidationServiceImpl::RecordTimer(const std::string& metric, int milliseconds) {
    if (statsd_ && statsd_->isConnected()) {
        statsd_->timing(metric, milliseconds);
    }
}

std::string ValidationServiceImpl::ComputeHash(const std::string& content) {
    std::hash<std::string> hasher;
    size_t hash = hasher(content);

    std::ostringstream oss;
    oss << std::hex << hash;
    return oss.str();
}

}  // namespace validationservice