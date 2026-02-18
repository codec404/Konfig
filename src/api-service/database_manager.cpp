#include "api_service/database_manager.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace apiservice {

DatabaseManager::DatabaseManager(const PostgresConfig& config)
    : config_(config), initialized_(false) {}

DatabaseManager::~DatabaseManager() {
    Shutdown();
}

std::string DatabaseManager::BuildConnectionString() {
    std::ostringstream oss;
    oss << "host=" << config_.host << " port=" << config_.port << " dbname=" << config_.database
        << " user=" << config_.user << " password=" << config_.password
        << " connect_timeout=" << config_.connection_timeout_seconds;
    return oss.str();
}

bool DatabaseManager::Initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        conn_ = std::make_unique<pqxx::connection>(BuildConnectionString());

        if (!conn_->is_open()) {
            std::cerr << "[DB] Failed to open connection" << std::endl;
            return false;
        }

        // Test connection
        pqxx::work txn(*conn_);
        txn.exec("SELECT 1");
        txn.commit();

        std::cout << "[DB] ✓ Connected to PostgreSQL" << std::endl;
        std::cout << "[DB]   Host:     " << config_.host << std::endl;
        std::cout << "[DB]   Database: " << config_.database << std::endl;

        initialized_ = true;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[DB] ✗ Connection failed: " << e.what() << std::endl;
        return false;
    }
}

void DatabaseManager::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    conn_.reset();
    initialized_ = false;
    std::cout << "[DB] Connection closed" << std::endl;
}

int64_t DatabaseManager::GetNextVersion(const std::string& service_name) {
    try {
        pqxx::work txn(*conn_);

        pqxx::result r = txn.exec_params("SELECT COALESCE(MAX(version), 0) + 1 "
                                         "FROM config_metadata "
                                         "WHERE service_name = $1",
                                         service_name);

        txn.commit();

        return r[0][0].as<int64_t>(1);

    } catch (const std::exception& e) {
        std::cerr << "[DB] GetNextVersion failed: " << e.what() << std::endl;
        return 1;
    }
}

std::pair<bool, std::string> DatabaseManager::InsertConfig(const configservice::ConfigData& config,
                                                           const std::string& description) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return {false, "Database not initialized"};
    }

    try {
        pqxx::work txn(*conn_);

        // Insert into config_metadata
        txn.exec_params("INSERT INTO config_metadata "
                        "  (config_id, service_name, version, format, "
                        "   created_by, description, is_active) "
                        "VALUES ($1, $2, $3, $4, $5, $6, true)",
                        config.config_id(), config.service_name(), config.version(),
                        config.format(), config.created_by(), description);

        // Insert into config_data
        txn.exec_params("INSERT INTO config_data "
                        "  (config_id, content, content_hash, size_bytes) "
                        "VALUES ($1, $2, $3, $4)",
                        config.config_id(), config.content(), config.content_hash(),
                        static_cast<int64_t>(config.content().size()));

        txn.commit();

        std::cout << "[DB] Inserted config: " << config.config_id() << std::endl;

        return {true, config.config_id()};

    } catch (const std::exception& e) {
        std::cerr << "[DB] InsertConfig failed: " << e.what() << std::endl;
        return {false, e.what()};
    }
}

configservice::ConfigData DatabaseManager::GetConfigById(const std::string& config_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        pqxx::work txn(*conn_);

        pqxx::result r =
            txn.exec_params("SELECT m.config_id, m.service_name, m.version, d.content, m.format, "
                            "       COALESCE(d.content_hash, '') as content_hash, "
                            "       m.created_at, m.created_by "
                            "FROM config_metadata m "
                            "JOIN config_data d ON m.config_id = d.config_id "
                            "WHERE m.config_id = $1",
                            config_id);

        txn.commit();

        if (r.empty()) {
            return configservice::ConfigData();
        }

        return ParseConfigRow(r[0]);

    } catch (const std::exception& e) {
        std::cerr << "[DB] GetConfigById failed: " << e.what() << std::endl;
        throw;
    }
}

configservice::ConfigData DatabaseManager::GetLatestConfig(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        pqxx::work txn(*conn_);

        pqxx::result r =
            txn.exec_params("SELECT m.config_id, m.service_name, m.version, d.content, m.format, "
                            "       COALESCE(d.content_hash, '') as content_hash, "
                            "       m.created_at, m.created_by "
                            "FROM config_metadata m "
                            "JOIN config_data d ON m.config_id = d.config_id "
                            "WHERE m.service_name = $1 "
                            "ORDER BY m.version DESC LIMIT 1",
                            service_name);

        txn.commit();

        if (r.empty()) {
            return configservice::ConfigData();
        }

        return ParseConfigRow(r[0]);

    } catch (const std::exception& e) {
        std::cerr << "[DB] GetLatestConfig failed: " << e.what() << std::endl;
        throw;
    }
}

configservice::ConfigData DatabaseManager::GetConfigByVersion(const std::string& service_name,
                                                              int64_t version) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        pqxx::work txn(*conn_);

        pqxx::result r =
            txn.exec_params("SELECT m.config_id, m.service_name, m.version, d.content, m.format, "
                            "       COALESCE(d.content_hash, '') as content_hash, "
                            "       m.created_at, m.created_by "
                            "FROM config_metadata m "
                            "JOIN config_data d ON m.config_id = d.config_id "
                            "WHERE m.service_name = $1 AND m.version = $2",
                            service_name, version);

        txn.commit();

        if (r.empty()) {
            return configservice::ConfigData();
        }

        return ParseConfigRow(r[0]);

    } catch (const std::exception& e) {
        std::cerr << "[DB] GetConfigByVersion failed: " << e.what() << std::endl;
        throw;
    }
}

std::vector<configservice::ConfigMetadata> DatabaseManager::ListConfigs(
    const std::string& service_name, int limit, int offset, int& total_count) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<configservice::ConfigMetadata> configs;

    try {
        pqxx::work txn(*conn_);

        pqxx::result r;
        pqxx::result count_r;

        if (service_name.empty()) {
            r = txn.exec_params("SELECT config_id, service_name, version, format, "
                                "       created_at, created_by, "
                                "       COALESCE(description, '') as description, is_active "
                                "FROM config_metadata "
                                "ORDER BY service_name, version DESC "
                                "LIMIT $1 OFFSET $2",
                                limit, offset);

            count_r = txn.exec("SELECT COUNT(*) FROM config_metadata");
        } else {
            r = txn.exec_params("SELECT config_id, service_name, version, format, "
                                "       created_at, created_by, "
                                "       COALESCE(description, '') as description, is_active "
                                "FROM config_metadata "
                                "WHERE service_name = $1 "
                                "ORDER BY version DESC "
                                "LIMIT $2 OFFSET $3",
                                service_name, limit, offset);

            count_r = txn.exec_params(
                "SELECT COUNT(*) FROM config_metadata WHERE service_name = $1", service_name);
        }

        txn.commit();

        total_count = count_r[0][0].as<int>(0);

        for (const auto& row : r) {
            configs.push_back(ParseMetadataRow(row));
        }

        return configs;

    } catch (const std::exception& e) {
        std::cerr << "[DB] ListConfigs failed: " << e.what() << std::endl;
        throw;
    }
}

std::pair<bool, std::string> DatabaseManager::DeleteConfigById(const std::string& config_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return {false, "Database not initialized"};
    }

    try {
        pqxx::work txn(*conn_);

        pqxx::result r = txn.exec_params("DELETE FROM config_metadata "
                                         "WHERE config_id = $1 "
                                         "RETURNING config_id, service_name",
                                         config_id);

        txn.commit();

        if (r.empty()) {
            return {false, "Config not found: " + config_id};
        }

        std::cout << "[DB] Deleted config: " << config_id << std::endl;

        return {true, "Deleted successfully"};

    } catch (const std::exception& e) {
        std::cerr << "[DB] DeleteConfig failed: " << e.what() << std::endl;
        return {false, e.what()};
    }
}

std::pair<bool, std::string> DatabaseManager::CreateRollout(const std::string& config_id,
                                                            configservice::RolloutStrategy strategy,
                                                            int32_t target_percentage) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return {false, "Database not initialized"};
    }

    try {
        pqxx::work txn(*conn_);

        // Generate rollout_id
        std::string rollout_id = "rollout-" + config_id;

        txn.exec_params(
            "INSERT INTO rollouts "
            "  (rollout_id, config_id, strategy, target_percentage, "
            "   current_percentage, status, started_at) "
            "VALUES ($1, $2, $3, $4, 0, 'IN_PROGRESS', EXTRACT(EPOCH FROM NOW())::BIGINT) "
            "ON CONFLICT (config_id) DO UPDATE "
            "SET strategy = $3, target_percentage = $4, "
            "    status = 'IN_PROGRESS', "
            "    started_at = EXTRACT(EPOCH FROM NOW())::BIGINT",
            rollout_id, config_id, static_cast<int>(strategy), target_percentage);

        txn.commit();

        std::cout << "[DB] Created rollout: " << rollout_id << std::endl;

        return {true, rollout_id};

    } catch (const std::exception& e) {
        std::cerr << "[DB] CreateRollout failed: " << e.what() << std::endl;
        return {false, e.what()};
    }
}

configservice::RolloutState DatabaseManager::GetRolloutState(const std::string& config_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    configservice::RolloutState state;

    try {
        pqxx::work txn(*conn_);

        pqxx::result r = txn.exec_params("SELECT config_id, strategy, target_percentage, "
                                         "       current_percentage, status, started_at, "
                                         "       COALESCE(completed_at, 0) as completed_at "
                                         "FROM rollouts "
                                         "WHERE config_id = $1",
                                         config_id);

        txn.commit();

        if (r.empty()) {
            state.set_config_id(config_id);
            state.set_status(configservice::RolloutStatus::PENDING);
            return state;
        }

        auto row = r[0];
        state.set_config_id(row["config_id"].as<std::string>());
        state.set_strategy(static_cast<configservice::RolloutStrategy>(row["strategy"].as<int>(0)));
        state.set_target_percentage(row["target_percentage"].as<int32_t>(100));
        state.set_current_percentage(row["current_percentage"].as<int32_t>(0));
        state.set_started_at(row["started_at"].as<int64_t>(0));
        state.set_completed_at(row["completed_at"].as<int64_t>(0));

        // Map status string to enum
        std::string status = row["status"].as<std::string>("PENDING");
        if (status == "IN_PROGRESS") {
            state.set_status(configservice::RolloutStatus::IN_PROGRESS);
        } else if (status == "COMPLETED") {
            state.set_status(configservice::RolloutStatus::COMPLETED);
        } else if (status == "FAILED") {
            state.set_status(configservice::RolloutStatus::FAILED);
        } else if (status == "ROLLED_BACK") {
            state.set_status(configservice::RolloutStatus::ROLLED_BACK);
        } else {
            state.set_status(configservice::RolloutStatus::PENDING);
        }

        return state;

    } catch (const std::exception& e) {
        std::cerr << "[DB] GetRolloutState failed: " << e.what() << std::endl;
        return state;
    }
}

std::vector<configservice::ServiceInstance> DatabaseManager::GetServiceInstances(
    const std::string& service_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<configservice::ServiceInstance> instances;

    try {
        pqxx::work txn(*conn_);

        pqxx::result r =
            txn.exec_params("SELECT service_name, instance_id, current_config_version, "
                            "       last_heartbeat, status "
                            "FROM service_instances "
                            "WHERE service_name = $1 "
                            "ORDER BY instance_id",
                            service_name);

        txn.commit();

        for (const auto& row : r) {
            configservice::ServiceInstance instance;
            instance.set_service_name(row["service_name"].as<std::string>());
            instance.set_instance_id(row["instance_id"].as<std::string>());
            instance.set_current_config_version(row["current_config_version"].as<int64_t>(0));
            instance.set_last_heartbeat(row["last_heartbeat"].as<int64_t>(0));
            instance.set_status(row["status"].as<std::string>("unknown"));
            instances.push_back(instance);
        }

        return instances;

    } catch (const std::exception& e) {
        std::cerr << "[DB] GetServiceInstances failed: " << e.what() << std::endl;
        return instances;
    }
}

void DatabaseManager::RecordAuditEvent(const std::string& service_name,
                                       const std::string& config_id, const std::string& action,
                                       const std::string& performed_by,
                                       const std::string& details) {
    try {
        pqxx::work txn(*conn_);

        txn.exec_params("INSERT INTO audit_log "
                        "  (config_id, action, performed_by, details) "
                        "VALUES ($1, $2, $3, jsonb_build_object('service_name', $4::text, "
                        "'details', $5::text))",
                        config_id, action, performed_by, service_name, details);

        txn.commit();

    } catch (const std::exception& e) {
        std::cerr << "[DB] RecordAuditEvent failed: " << e.what() << std::endl;
    }
}

configservice::ConfigData DatabaseManager::ParseConfigRow(const pqxx::row& row) {
    configservice::ConfigData config;
    config.set_config_id(row["config_id"].as<std::string>());
    config.set_service_name(row["service_name"].as<std::string>());
    config.set_version(row["version"].as<int64_t>());
    config.set_content(row["content"].as<std::string>());
    config.set_format(row["format"].as<std::string>());
    config.set_content_hash(row["content_hash"].as<std::string>(""));

    // Convert PostgreSQL TIMESTAMP to Unix timestamp
    if (!row["created_at"].is_null()) {
        auto ts = row["created_at"].as<std::string>();
        std::tm tm = {};
        std::istringstream ss(ts);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        config.set_created_at(ss.fail() ? 0 : static_cast<int64_t>(std::mktime(&tm)));
    } else {
        config.set_created_at(0);
    }

    config.set_created_by(row["created_by"].as<std::string>());
    return config;
}

configservice::ConfigMetadata DatabaseManager::ParseMetadataRow(const pqxx::row& row) {
    configservice::ConfigMetadata meta;
    meta.set_config_id(row["config_id"].as<std::string>());
    meta.set_service_name(row["service_name"].as<std::string>());
    meta.set_version(row["version"].as<int64_t>());
    meta.set_format(row["format"].as<std::string>());

    if (!row["created_at"].is_null()) {
        auto ts = row["created_at"].as<std::string>();
        std::tm tm = {};
        std::istringstream ss(ts);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        meta.set_created_at(ss.fail() ? 0 : static_cast<int64_t>(std::mktime(&tm)));
    } else {
        meta.set_created_at(0);
    }

    meta.set_created_by(row["created_by"].as<std::string>());
    meta.set_description(row["description"].as<std::string>(""));
    meta.set_is_active(row["is_active"].as<bool>(true));
    return meta;
}

}  // namespace apiservice