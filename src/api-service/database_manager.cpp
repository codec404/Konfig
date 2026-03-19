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

int64_t DatabaseManager::GetNextVersion(const std::string& service_name,
                                        const std::string& config_name) {
    try {
        pqxx::work txn(*conn_);

        pqxx::result r = txn.exec_params("SELECT COALESCE(MAX(version), 0) + 1 "
                                         "FROM config_metadata "
                                         "WHERE service_name = $1 AND config_name = $2",
                                         service_name, config_name);

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

        // v1 of a named config is auto-activated; later versions stay inactive
        // until a rollout promotes them.
        pqxx::result existing = txn.exec_params("SELECT COUNT(*) FROM config_metadata "
                                                "WHERE service_name = $1 AND config_name = $2",
                                                config.service_name(), config.config_name());
        bool is_first = existing[0][0].as<int64_t>() == 0;

        txn.exec_params("INSERT INTO config_metadata "
                        "  (config_id, service_name, config_name, version, format, "
                        "   created_by, description, is_active) "
                        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8)",
                        config.config_id(), config.service_name(), config.config_name(),
                        config.version(), config.format(), config.created_by(), description,
                        is_first);

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
            txn.exec_params("SELECT m.config_id, m.service_name, m.config_name, m.version, "
                            "       d.content, m.format, "
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

configservice::ConfigData DatabaseManager::GetLatestConfigByName(const std::string& service_name,
                                                                 const std::string& config_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        pqxx::work txn(*conn_);

        pqxx::result r =
            txn.exec_params("SELECT m.config_id, m.service_name, m.config_name, m.version, "
                            "       d.content, m.format, "
                            "       COALESCE(d.content_hash, '') as content_hash, "
                            "       m.created_at, m.created_by "
                            "FROM config_metadata m "
                            "JOIN config_data d ON m.config_id = d.config_id "
                            "WHERE m.service_name = $1 AND m.config_name = $2 "
                            "ORDER BY m.version DESC LIMIT 1",
                            service_name, config_name);

        txn.commit();

        if (r.empty()) {
            return configservice::ConfigData();
        }

        return ParseConfigRow(r[0]);

    } catch (const std::exception& e) {
        std::cerr << "[DB] GetLatestConfigByName failed: " << e.what() << std::endl;
        throw;
    }
}

configservice::ConfigData DatabaseManager::GetActiveConfig(const std::string& service_name,
                                                           const std::string& config_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        pqxx::work txn(*conn_);

        pqxx::result r =
            txn.exec_params("SELECT m.config_id, m.service_name, m.config_name, m.version, "
                            "       d.content, m.format, "
                            "       COALESCE(d.content_hash, '') as content_hash, "
                            "       m.created_at, m.created_by "
                            "FROM config_metadata m "
                            "JOIN config_data d ON m.config_id = d.config_id "
                            "WHERE m.service_name = $1 AND m.config_name = $2 "
                            "  AND m.is_active = true "
                            "LIMIT 1",
                            service_name, config_name);

        txn.commit();

        if (r.empty()) {
            return configservice::ConfigData();
        }

        return ParseConfigRow(r[0]);

    } catch (const std::exception& e) {
        std::cerr << "[DB] GetActiveConfig failed: " << e.what() << std::endl;
        throw;
    }
}

void DatabaseManager::SetActiveConfig(const std::string& service_name,
                                      const std::string& config_name,
                                      const std::string& config_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        pqxx::work txn(*conn_);

        // Only deactivate within the same named config, not the whole service
        txn.exec_params("UPDATE config_metadata SET is_active = false "
                        "WHERE service_name = $1 AND config_name = $2",
                        service_name, config_name);
        txn.exec_params("UPDATE config_metadata SET is_active = true WHERE config_id = $1",
                        config_id);
        txn.commit();

        std::cout << "[DB] SetActiveConfig: " << config_id << " is now active" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[DB] SetActiveConfig failed: " << e.what() << std::endl;
        throw;
    }
}

configservice::ConfigData DatabaseManager::GetConfigByVersion(const std::string& service_name,
                                                              const std::string& config_name,
                                                              int64_t version) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        pqxx::work txn(*conn_);

        pqxx::result r =
            txn.exec_params("SELECT m.config_id, m.service_name, m.config_name, m.version, "
                            "       d.content, m.format, "
                            "       COALESCE(d.content_hash, '') as content_hash, "
                            "       m.created_at, m.created_by "
                            "FROM config_metadata m "
                            "JOIN config_data d ON m.config_id = d.config_id "
                            "WHERE m.service_name = $1 AND m.config_name = $2 "
                            "  AND m.version = $3",
                            service_name, config_name, version);

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
    const std::string& service_name, const std::string& config_name, int limit, int offset,
    int& total_count) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<configservice::ConfigMetadata> configs;

    try {
        pqxx::work txn(*conn_);

        pqxx::result r =
            txn.exec_params("SELECT config_id, service_name, config_name, version, format, "
                            "       created_at, created_by, "
                            "       COALESCE(description, '') as description, is_active "
                            "FROM config_metadata "
                            "WHERE service_name = $1 AND config_name = $2 "
                            "ORDER BY version DESC "
                            "LIMIT $3 OFFSET $4",
                            service_name, config_name, limit, offset);

        pqxx::result count_r = txn.exec_params("SELECT COUNT(*) FROM config_metadata "
                                               "WHERE service_name = $1 AND config_name = $2",
                                               service_name, config_name);

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

std::vector<configservice::NamedConfigSummary> DatabaseManager::ListNamedConfigs(
    const std::string& service_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<configservice::NamedConfigSummary> summaries;

    try {
        pqxx::work txn(*conn_);

        pqxx::result r =
            txn.exec_params("SELECT cm.service_name, cm.config_name, "
                            "       MAX(cm.format) AS format, "
                            "       COUNT(*) AS version_count, "
                            "       MAX(cm.version) AS latest_version, "
                            "       MAX(cm.created_at)::text AS latest_updated_at, "
                            "       EXISTS( "
                            "         SELECT 1 FROM rollout_state rs "
                            "         JOIN config_metadata cm2 ON cm2.config_id = rs.config_id "
                            "         WHERE cm2.service_name = cm.service_name "
                            "           AND cm2.config_name = cm.config_name "
                            "           AND rs.status IN ('IN_PROGRESS', 'PENDING') "
                            "       ) AS has_active_rollout "
                            "FROM config_metadata cm "
                            "WHERE cm.service_name = $1 "
                            "GROUP BY cm.service_name, cm.config_name "
                            "ORDER BY cm.config_name",
                            service_name);

        txn.commit();

        for (const auto& row : r) {
            configservice::NamedConfigSummary s;
            s.set_service_name(row["service_name"].as<std::string>());
            s.set_config_name(row["config_name"].as<std::string>());
            s.set_format(row["format"].as<std::string>("json"));
            s.set_version_count(row["version_count"].as<int32_t>(0));
            s.set_latest_version(row["latest_version"].as<int64_t>(0));
            if (!row["latest_updated_at"].is_null()) {
                s.set_latest_updated_at(row["latest_updated_at"].as<std::string>());
            }
            s.set_has_active_rollout(row["has_active_rollout"].as<bool>(false));
            summaries.push_back(s);
        }

        return summaries;

    } catch (const std::exception& e) {
        std::cerr << "[DB] ListNamedConfigs failed: " << e.what() << std::endl;
        return summaries;
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

std::pair<bool, std::string> DatabaseManager::PromoteRollout(const std::string& config_id,
                                                             int32_t new_target_percentage) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return {false, "Database not initialized"};
    }

    try {
        pqxx::work txn(*conn_);

        auto r = txn.exec_params(
            "SELECT status, target_percentage FROM rollout_state WHERE config_id = $1", config_id);

        if (r.empty()) {
            return {false, "No rollout found for config: " + config_id};
        }

        std::string status = r[0]["status"].as<std::string>();
        if (status != "IN_PROGRESS" && status != "PENDING") {
            return {false, "Rollout is not active (status: " + status + ")"};
        }

        int32_t current_target = r[0]["target_percentage"].as<int32_t>(0);
        if (new_target_percentage <= current_target) {
            return {false, "New target (" + std::to_string(new_target_percentage) +
                               "%) must be greater than current target (" +
                               std::to_string(current_target) + "%)"};
        }

        txn.exec_params("UPDATE rollout_state SET target_percentage = $1 WHERE config_id = $2",
                        new_target_percentage, config_id);

        txn.commit();

        std::cout << "[DB] Promoted rollout " << config_id << " to " << new_target_percentage << "%"
                  << std::endl;

        return {true, "Rollout promoted to " + std::to_string(new_target_percentage) + "%"};

    } catch (const std::exception& e) {
        std::cerr << "[DB] PromoteRollout failed: " << e.what() << std::endl;
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

        // Supersede any existing active rollouts for the same service
        txn.exec_params(
            "UPDATE rollout_state SET status = 'COMPLETED', completed_at = NOW() "
            "WHERE config_id IN ("
            "  SELECT config_id FROM config_metadata "
            "  WHERE service_name = (SELECT service_name FROM config_metadata WHERE config_id = $1)"
            ") AND status IN ('IN_PROGRESS', 'PENDING') AND config_id != $1",
            config_id);

        txn.exec_params("INSERT INTO rollout_state "
                        "  (config_id, strategy, target_percentage, "
                        "   current_percentage, status, started_at) "
                        "VALUES ($1, $2, $3, 0, 'IN_PROGRESS', NOW()) "
                        "ON CONFLICT (config_id) DO UPDATE "
                        "SET strategy = $2, target_percentage = $3, "
                        "    status = 'IN_PROGRESS', "
                        "    started_at = NOW()",
                        config_id, static_cast<int>(strategy), target_percentage);

        txn.commit();

        std::cout << "[DB] Created rollout for config: " << config_id << std::endl;

        return {true, config_id};

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

        pqxx::result r =
            txn.exec_params("SELECT config_id, strategy, target_percentage, "
                            "       current_percentage, status, "
                            "       EXTRACT(EPOCH FROM started_at)::BIGINT as started_at, "
                            "       EXTRACT(EPOCH FROM COALESCE(completed_at, TIMESTAMP "
                            "'epoch'))::BIGINT as completed_at "
                            "FROM rollout_state "
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
                            "       EXTRACT(EPOCH FROM last_heartbeat)::BIGINT as last_heartbeat, "
                            "       status "
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
    if (row.column_number("config_name") >= 0) {
        config.set_config_name(row["config_name"].as<std::string>("default"));
    }
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
    if (row.column_number("config_name") >= 0) {
        meta.set_config_name(row["config_name"].as<std::string>("default"));
    }
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

std::vector<configservice::AuditEntry> DatabaseManager::GetAuditLog(const std::string& service_name,
                                                                    int limit) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<configservice::AuditEntry> entries;

    try {
        pqxx::work txn(*conn_);
        pqxx::result r;

        if (!service_name.empty()) {
            r = txn.exec_params("SELECT id, config_id, action, performed_by, "
                                "       details->>'service_name' AS service_name, "
                                "       details->>'details' AS detail_text, "
                                "       EXTRACT(EPOCH FROM created_at)::bigint AS created_at_unix "
                                "FROM audit_log "
                                "WHERE details->>'service_name' = $1 "
                                "ORDER BY created_at DESC LIMIT $2",
                                service_name, limit > 0 ? limit : 20);
        } else {
            r = txn.exec_params("SELECT id, config_id, action, performed_by, "
                                "       details->>'service_name' AS service_name, "
                                "       details->>'details' AS detail_text, "
                                "       EXTRACT(EPOCH FROM created_at)::bigint AS created_at_unix "
                                "FROM audit_log "
                                "ORDER BY created_at DESC LIMIT $1",
                                limit > 0 ? limit : 20);
        }

        txn.commit();

        for (const auto& row : r) {
            configservice::AuditEntry entry;
            entry.set_id(row["id"].as<int64_t>(0));
            entry.set_config_id(row["config_id"].as<std::string>(""));
            entry.set_action(row["action"].as<std::string>(""));
            entry.set_performed_by(row["performed_by"].as<std::string>(""));
            entry.set_service_name(
                row["service_name"].is_null() ? "" : row["service_name"].as<std::string>());
            entry.set_details(row["detail_text"].is_null() ? ""
                                                           : row["detail_text"].as<std::string>());
            entry.set_created_at(row["created_at_unix"].as<int64_t>(0));
            entries.push_back(entry);
        }

        return entries;

    } catch (const std::exception& e) {
        std::cerr << "[DB] GetAuditLog failed: " << e.what() << std::endl;
        return entries;
    }
}

configservice::KonfigStats DatabaseManager::GetStats() {
    std::lock_guard<std::mutex> lock(mutex_);

    configservice::KonfigStats stats;

    try {
        pqxx::work txn(*conn_);

        // Total configs
        auto r1 = txn.exec("SELECT COUNT(*) FROM config_metadata");
        stats.set_total_configs(r1[0][0].as<int32_t>(0));

        // Total services
        auto r2 = txn.exec("SELECT COUNT(DISTINCT service_name) FROM config_metadata");
        stats.set_total_services(r2[0][0].as<int32_t>(0));

        // Active rollouts (IN_PROGRESS or PENDING)
        auto r3 = txn.exec(
            "SELECT COUNT(*) FROM rollout_state WHERE status IN ('IN_PROGRESS', 'PENDING')");
        stats.set_active_rollouts(r3[0][0].as<int32_t>(0));

        // Total schemas
        auto r4 = txn.exec("SELECT COUNT(*) FROM validation_schemas WHERE is_active = true");
        stats.set_total_schemas(r4[0][0].as<int32_t>(0));

        // Connected instances (heartbeat within last 120 seconds)
        auto r5 = txn.exec("SELECT COUNT(*) FROM service_instances "
                           "WHERE last_heartbeat > NOW() - INTERVAL '120 seconds'");
        stats.set_connected_instances(r5[0][0].as<int32_t>(0));

        txn.commit();

        return stats;

    } catch (const std::exception& e) {
        std::cerr << "[DB] GetStats failed: " << e.what() << std::endl;
        return stats;
    }
}

std::vector<configservice::ServiceSummary> DatabaseManager::ListServices() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<configservice::ServiceSummary> services;

    try {
        pqxx::work txn(*conn_);

        pqxx::result r = txn.exec("SELECT "
                                  "  cm.service_name, "
                                  "  MAX(cm.version) AS latest_version, "
                                  "  COUNT(DISTINCT cm.config_name) AS config_count, "
                                  "  MAX(cm.created_at) AS latest_updated_at, "
                                  "  EXISTS( "
                                  "    SELECT 1 FROM rollout_state rs "
                                  "    JOIN config_metadata cm2 ON cm2.config_id = rs.config_id "
                                  "    WHERE cm2.service_name = cm.service_name "
                                  "      AND rs.status IN ('IN_PROGRESS', 'PENDING') "
                                  "  ) AS has_active_rollout "
                                  "FROM config_metadata cm "
                                  "GROUP BY cm.service_name "
                                  "ORDER BY cm.service_name");

        txn.commit();

        for (const auto& row : r) {
            configservice::ServiceSummary svc;
            svc.set_service_name(row["service_name"].as<std::string>());
            svc.set_latest_version(row["latest_version"].as<int64_t>(0));
            svc.set_config_count(row["config_count"].as<int32_t>(0));
            if (!row["latest_updated_at"].is_null()) {
                svc.set_latest_updated_at(row["latest_updated_at"].as<std::string>());
            }
            svc.set_has_active_rollout(row["has_active_rollout"].as<bool>(false));
            services.push_back(svc);
        }

        return services;

    } catch (const std::exception& e) {
        std::cerr << "[DB] ListServices failed: " << e.what() << std::endl;
        return services;
    }
}

std::vector<configservice::RolloutSummary> DatabaseManager::ListRollouts(
    const std::string& status_filter, int limit) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<configservice::RolloutSummary> rollouts;

    try {
        pqxx::work txn(*conn_);
        pqxx::result r;

        int lim = limit > 0 ? limit : 50;

        if (status_filter == "ACTIVE") {
            r = txn.exec_params(
                "SELECT rs.config_id, COALESCE(cm.service_name, '') AS service_name, "
                "       rs.strategy, rs.target_percentage, rs.current_percentage, "
                "       rs.status, rs.started_at, rs.completed_at "
                "FROM rollout_state rs "
                "LEFT JOIN config_metadata cm ON cm.config_id = rs.config_id "
                "WHERE rs.status IN ('IN_PROGRESS', 'PENDING') "
                "ORDER BY rs.started_at DESC LIMIT $1",
                lim);
        } else if (!status_filter.empty()) {
            r = txn.exec_params(
                "SELECT rs.config_id, COALESCE(cm.service_name, '') AS service_name, "
                "       rs.strategy, rs.target_percentage, rs.current_percentage, "
                "       rs.status, rs.started_at, rs.completed_at "
                "FROM rollout_state rs "
                "LEFT JOIN config_metadata cm ON cm.config_id = rs.config_id "
                "WHERE rs.status = $1 "
                "ORDER BY rs.started_at DESC LIMIT $2",
                status_filter, lim);
        } else {
            r = txn.exec_params(
                "SELECT rs.config_id, COALESCE(cm.service_name, '') AS service_name, "
                "       rs.strategy, rs.target_percentage, rs.current_percentage, "
                "       rs.status, rs.started_at, rs.completed_at "
                "FROM rollout_state rs "
                "LEFT JOIN config_metadata cm ON cm.config_id = rs.config_id "
                "ORDER BY rs.started_at DESC LIMIT $1",
                lim);
        }

        txn.commit();

        for (const auto& row : r) {
            configservice::RolloutSummary rs;
            rs.set_config_id(row["config_id"].as<std::string>(""));
            rs.set_service_name(row["service_name"].as<std::string>(""));
            rs.set_strategy(row["strategy"].as<std::string>(""));
            rs.set_target_percentage(row["target_percentage"].as<int32_t>(100));
            rs.set_current_percentage(row["current_percentage"].as<int32_t>(0));
            rs.set_status(row["status"].as<std::string>(""));
            if (!row["started_at"].is_null())
                rs.set_started_at(row["started_at"].as<std::string>());
            if (!row["completed_at"].is_null())
                rs.set_completed_at(row["completed_at"].as<std::string>());
            rollouts.push_back(rs);
        }

        return rollouts;

    } catch (const std::exception& e) {
        std::cerr << "[DB] ListRollouts failed: " << e.what() << std::endl;
        return rollouts;
    }
}

}  // namespace apiservice