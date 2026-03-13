#include "distribution_service/database_manager.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace configservice {

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
        pqxx::result r = txn.exec("SELECT version()");
        txn.commit();

        std::cout << "[DB] ✓ Connected to PostgreSQL" << std::endl;
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

ConfigData DatabaseManager::GetLatestConfig(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        throw std::runtime_error("Database not initialized");
    }

    try {
        pqxx::work txn(*conn_);

        pqxx::result r =
            txn.exec_params("SELECT m.config_id, m.service_name, m.version, m.format, d.content, "
                            "       m.created_at, m.created_by "
                            "FROM config_metadata m "
                            "JOIN config_data d ON m.config_id = d.config_id "
                            "WHERE m.service_name = $1 "
                            "ORDER BY m.version DESC LIMIT 1",
                            service_name);

        if (r.empty()) {
            // Return empty config
            ConfigData config;
            config.set_service_name(service_name);
            config.set_version(0);
            return config;
        }

        auto result = ParseConfigRow(r[0]);
        txn.commit();

        std::cout << "[DB] Fetched config: " << service_name << " v" << result.version()
                  << std::endl;

        return result;

    } catch (const std::exception& e) {
        std::cerr << "[DB] Query failed: " << e.what() << std::endl;
        throw;
    }
}

ConfigData DatabaseManager::GetLatestRolledOutConfig(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        throw std::runtime_error("Database not initialized");
    }

    try {
        pqxx::work txn(*conn_);

        // Latest version that has a COMPLETED rollout
        pqxx::result r =
            txn.exec_params("SELECT m.config_id, m.service_name, m.version, m.format, d.content, "
                            "       m.created_at, m.created_by "
                            "FROM config_metadata m "
                            "JOIN config_data d ON m.config_id = d.config_id "
                            "JOIN rollout_state rs ON rs.config_id = m.config_id "
                            "WHERE m.service_name = $1 AND rs.status = 'COMPLETED' "
                            "ORDER BY m.version DESC LIMIT 1",
                            service_name);

        if (r.empty()) {
            // No completed rollout — fall back to absolute latest
            // (handles first-time setup before any rollout has been run)
            pqxx::result r2 = txn.exec_params(
                "SELECT m.config_id, m.service_name, m.version, m.format, d.content, "
                "       m.created_at, m.created_by "
                "FROM config_metadata m "
                "JOIN config_data d ON m.config_id = d.config_id "
                "WHERE m.service_name = $1 "
                "ORDER BY m.version DESC LIMIT 1",
                service_name);

            if (r2.empty()) {
                ConfigData config;
                config.set_service_name(service_name);
                config.set_version(0);
                return config;
            }
            auto result = ParseConfigRow(r2[0]);
            txn.commit();
            return result;
        }

        auto result = ParseConfigRow(r[0]);
        txn.commit();

        std::cout << "[DB] Latest rolled-out config: " << service_name << " v" << result.version()
                  << std::endl;
        return result;

    } catch (const std::exception& e) {
        std::cerr << "[DB] GetLatestRolledOutConfig failed: " << e.what() << std::endl;
        throw;
    }
}

ConfigData DatabaseManager::GetConfigByVersion(const std::string& service_name, int64_t version) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        throw std::runtime_error("Database not initialized");
    }

    try {
        pqxx::work txn(*conn_);

        pqxx::result r =
            txn.exec_params("SELECT m.config_id, m.service_name, m.version, m.format, d.content, "
                            "       m.created_at, m.created_by "
                            "FROM config_metadata m "
                            "JOIN config_data d ON m.config_id = d.config_id "
                            "WHERE m.service_name = $1 AND m.version = $2",
                            service_name, version);

        if (r.empty()) {
            ConfigData config;
            config.set_service_name(service_name);
            config.set_version(0);
            return config;
        }

        auto result = ParseConfigRow(r[0]);
        txn.commit();

        return result;

    } catch (const std::exception& e) {
        std::cerr << "[DB] Query failed: " << e.what() << std::endl;
        throw;
    }
}

std::vector<ConfigData> DatabaseManager::ListConfigs(const std::string& service_name, int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ConfigData> configs;

    if (!initialized_) {
        throw std::runtime_error("Database not initialized");
    }

    try {
        pqxx::work txn(*conn_);

        pqxx::result r;
        if (service_name.empty()) {
            r = txn.exec_params(
                "SELECT DISTINCT ON (m.service_name) "
                "       m.config_id, m.service_name, m.version, m.format, d.content, "
                "       m.created_at, m.created_by "
                "FROM config_metadata m "
                "JOIN config_data d ON m.config_id = d.config_id "
                "ORDER BY m.service_name, m.version DESC "
                "LIMIT $1",
                limit);
        } else {
            r = txn.exec_params(
                "SELECT m.config_id, m.service_name, m.version, m.format, d.content, "
                "       m.created_at, m.created_by "
                "FROM config_metadata m "
                "JOIN config_data d ON m.config_id = d.config_id "
                "WHERE m.service_name = $1 "
                "ORDER BY m.version DESC "
                "LIMIT $2",
                service_name, limit);
        }

        for (const auto& row : r) {
            configs.push_back(ParseConfigRow(row));
        }

        txn.commit();

        return configs;

    } catch (const std::exception& e) {
        std::cerr << "[DB] Query failed: " << e.what() << std::endl;
        throw;
    }
}

bool DatabaseManager::UpdateClientStatus(const std::string& service_name,
                                         const std::string& instance_id, int64_t version,
                                         const std::string& status) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return false;
    }

    try {
        pqxx::work txn(*conn_);

        txn.exec_params(
            "INSERT INTO service_instances "
            "  (service_name, instance_id, current_config_version, last_heartbeat, status) "
            "VALUES ($1, $2, $3, NOW(), $4) "
            "ON CONFLICT (service_name, instance_id) DO UPDATE "
            "SET current_config_version = $3, last_heartbeat = NOW(), status = $4",
            service_name, instance_id, version, status);

        txn.commit();
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[DB] Update failed: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseManager::RecordConfigDelivery(const std::string& service_name,
                                           const std::string& instance_id, int64_t version) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return false;
    }

    try {
        pqxx::work txn(*conn_);

        txn.exec_params("INSERT INTO audit_log "
                        "  (config_id, action, performed_by, details) "
                        "VALUES ($1, 'delivered', 'distribution-service', "
                        "jsonb_build_object('service_name', $2::text, 'instance_id', $3::text))",
                        "cfg-" + service_name + "-v" + std::to_string(version), service_name,
                        instance_id);

        txn.commit();
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[DB] Audit record failed: " << e.what() << std::endl;
        return false;
    }
}

ConfigData DatabaseManager::GetConfigById(const std::string& config_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        throw std::runtime_error("Database not initialized");
    }

    try {
        pqxx::work txn(*conn_);

        pqxx::result r =
            txn.exec_params("SELECT m.config_id, m.service_name, m.version, m.format, d.content, "
                            "       m.created_at, m.created_by "
                            "FROM config_metadata m "
                            "JOIN config_data d ON m.config_id = d.config_id "
                            "WHERE m.config_id = $1",
                            config_id);

        if (r.empty()) {
            ConfigData config;
            config.set_config_id(config_id);
            config.set_version(0);
            return config;
        }

        auto result = ParseConfigRow(r[0]);
        txn.commit();
        return result;

    } catch (const std::exception& e) {
        std::cerr << "[DB] GetConfigById failed: " << e.what() << std::endl;
        throw;
    }
}

RolloutInfo DatabaseManager::GetRolloutInfo(const std::string& config_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    RolloutInfo info;

    if (!initialized_) {
        return info;
    }

    try {
        pqxx::work txn(*conn_);

        pqxx::result r = txn.exec_params("SELECT strategy, target_percentage, status "
                                         "FROM rollout_state WHERE config_id = $1",
                                         config_id);

        if (!r.empty()) {
            info.strategy = r[0]["strategy"].as<int>();
            info.target_percentage = r[0]["target_percentage"].as<int32_t>();
            info.status = r[0]["status"].as<std::string>();
            info.found = true;
        }

        txn.commit();

    } catch (const std::exception& e) {
        std::cerr << "[DB] GetRolloutInfo failed: " << e.what() << std::endl;
    }

    return info;
}

bool DatabaseManager::UpdateRolloutProgress(const std::string& config_id, int32_t current_pct,
                                            const std::string& status) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return false;
    }

    try {
        pqxx::work txn(*conn_);

        std::string sql = "UPDATE rollout_state "
                          "SET current_percentage = $2, status = $3";
        if (status == "COMPLETED" || status == "FAILED") {
            sql += ", completed_at = NOW()";
        }
        sql += " WHERE config_id = $1";

        txn.exec_params(sql, config_id, current_pct, status);

        // On successful completion, mark this config active and deactivate others
        if (status == "COMPLETED") {
            txn.exec_params("UPDATE config_metadata SET is_active = false "
                            "WHERE service_name = (SELECT service_name FROM config_metadata WHERE "
                            "config_id = $1) "
                            "AND config_id != $1",
                            config_id);
            txn.exec_params("UPDATE config_metadata SET is_active = true WHERE config_id = $1",
                            config_id);
        }

        txn.commit();

        std::cout << "[DB] Rollout progress: " << config_id << " → " << current_pct << "% ("
                  << status << ")" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[DB] UpdateRolloutProgress failed: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::pair<std::string, std::string>> DatabaseManager::GetPendingRollouts() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::pair<std::string, std::string>> result;

    if (!initialized_)
        return result;

    try {
        pqxx::work txn(*conn_);

        pqxx::result r = txn.exec("SELECT rs.config_id, cm.service_name "
                                  "FROM rollout_state rs "
                                  "JOIN config_metadata cm ON rs.config_id = cm.config_id "
                                  "WHERE rs.status = 'IN_PROGRESS' "
                                  "ORDER BY rs.started_at ASC");

        for (const auto& row : r) {
            result.emplace_back(row["config_id"].as<std::string>(),
                                row["service_name"].as<std::string>());
        }

        txn.commit();

    } catch (const std::exception& e) {
        std::cerr << "[DB] GetPendingRollouts failed: " << e.what() << std::endl;
    }

    return result;
}

ConfigData DatabaseManager::ParseConfigRow(const pqxx::row& row) {
    ConfigData config;

    config.set_config_id(row["config_id"].as<std::string>());
    config.set_service_name(row["service_name"].as<std::string>());
    config.set_version(row["version"].as<int64_t>());
    config.set_format(row["format"].as<std::string>());
    config.set_content(row["content"].as<std::string>());

    // Convert PostgreSQL TIMESTAMP to Unix timestamp
    if (!row["created_at"].is_null()) {
        auto timestamp_str = row["created_at"].as<std::string>();
        // Parse PostgreSQL timestamp format: "YYYY-MM-DD HH:MM:SS.microseconds"
        std::tm tm = {};
        std::istringstream ss(timestamp_str);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        if (!ss.fail()) {
            auto time = std::mktime(&tm);
            config.set_created_at(static_cast<int64_t>(time));
        } else {
            config.set_created_at(0);  // Fallback to epoch
        }
    } else {
        config.set_created_at(0);
    }

    config.set_created_by(row["created_by"].as<std::string>());

    return config;
}

}  // namespace configservice