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