#include "validation_service/database_manager.h"

#include <ctime>
#include <iostream>
#include <sstream>

namespace validationservice {

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

        pqxx::work txn(*conn_);
        txn.exec("SELECT 1");
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

std::pair<bool, std::string> DatabaseManager::RegisterSchema(
    const configservice::ValidationSchema& schema) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return {false, "Database not initialized"};
    }

    try {
        pqxx::work txn(*conn_);

        txn.exec_params("INSERT INTO validation_schemas "
                        "  (schema_id, service_name, schema_type, schema_content, "
                        "   description, created_by, created_at, is_active) "
                        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8) "
                        "ON CONFLICT (schema_id) DO UPDATE "
                        "SET schema_content = $4, description = $5, "
                        "    updated_at = $7, is_active = $8",
                        schema.schema_id(), schema.service_name(), schema.schema_type(),
                        schema.schema_content(), schema.description(), schema.created_by(),
                        schema.created_at(), schema.is_active());

        txn.commit();

        std::cout << "[DB] Registered schema: " << schema.schema_id() << std::endl;

        return {true, schema.schema_id()};

    } catch (const std::exception& e) {
        std::cerr << "[DB] RegisterSchema failed: " << e.what() << std::endl;
        return {false, e.what()};
    }
}

configservice::ValidationSchema DatabaseManager::GetSchema(const std::string& schema_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    configservice::ValidationSchema schema;

    try {
        pqxx::work txn(*conn_);

        pqxx::result r =
            txn.exec_params("SELECT schema_id, service_name, schema_type, schema_content, "
                            "       COALESCE(description, '') as description, "
                            "       COALESCE(created_by, '') as created_by, "
                            "       created_at, is_active "
                            "FROM validation_schemas "
                            "WHERE schema_id = $1",
                            schema_id);

        txn.commit();

        if (r.empty()) {
            return schema;
        }

        auto row = r[0];
        schema.set_schema_id(row["schema_id"].as<std::string>());
        schema.set_service_name(row["service_name"].as<std::string>());
        schema.set_schema_type(row["schema_type"].as<std::string>());
        schema.set_schema_content(row["schema_content"].as<std::string>());
        schema.set_description(row["description"].as<std::string>());
        schema.set_created_by(row["created_by"].as<std::string>());
        schema.set_created_at(row["created_at"].as<int64_t>());
        schema.set_is_active(row["is_active"].as<bool>());

        return schema;

    } catch (const std::exception& e) {
        std::cerr << "[DB] GetSchema failed: " << e.what() << std::endl;
        return schema;
    }
}

std::vector<configservice::ValidationSchema> DatabaseManager::ListSchemas(
    const std::string& service_name, int limit, int offset, int& total_count) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<configservice::ValidationSchema> schemas;

    try {
        pqxx::work txn(*conn_);

        pqxx::result r;
        pqxx::result count_r;

        if (service_name.empty()) {
            r = txn.exec_params("SELECT schema_id, service_name, schema_type, schema_content, "
                                "       COALESCE(description, '') as description, "
                                "       COALESCE(created_by, '') as created_by, "
                                "       created_at, is_active "
                                "FROM validation_schemas "
                                "ORDER BY created_at DESC "
                                "LIMIT $1 OFFSET $2",
                                limit, offset);

            count_r = txn.exec("SELECT COUNT(*) FROM validation_schemas");
        } else {
            r = txn.exec_params("SELECT schema_id, service_name, schema_type, schema_content, "
                                "       COALESCE(description, '') as description, "
                                "       COALESCE(created_by, '') as created_by, "
                                "       created_at, is_active "
                                "FROM validation_schemas "
                                "WHERE service_name = $1 "
                                "ORDER BY created_at DESC "
                                "LIMIT $2 OFFSET $3",
                                service_name, limit, offset);

            count_r = txn.exec_params(
                "SELECT COUNT(*) FROM validation_schemas WHERE service_name = $1", service_name);
        }

        txn.commit();

        total_count = count_r[0][0].as<int>(0);

        for (const auto& row : r) {
            configservice::ValidationSchema schema;
            schema.set_schema_id(row["schema_id"].as<std::string>());
            schema.set_service_name(row["service_name"].as<std::string>());
            schema.set_schema_type(row["schema_type"].as<std::string>());
            schema.set_schema_content(row["schema_content"].as<std::string>());
            schema.set_description(row["description"].as<std::string>());
            schema.set_created_by(row["created_by"].as<std::string>());
            schema.set_created_at(row["created_at"].as<int64_t>());
            schema.set_is_active(row["is_active"].as<bool>());
            schemas.push_back(schema);
        }

        return schemas;

    } catch (const std::exception& e) {
        std::cerr << "[DB] ListSchemas failed: " << e.what() << std::endl;
        return schemas;
    }
}

void DatabaseManager::RecordValidation(const std::string& service_name, const std::string& content,
                                       bool result, const std::string& errors,
                                       const std::string& warnings,
                                       const std::string& validated_by) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return;
    }

    try {
        pqxx::work txn(*conn_);

        txn.exec_params("INSERT INTO validation_history "
                        "  (service_name, config_content, validation_result, "
                        "   errors, warnings, validated_at, validated_by) "
                        "VALUES ($1, $2, $3, $4, $5, $6, $7)",
                        service_name, content, result, errors, warnings, std::time(nullptr),
                        validated_by);

        txn.commit();

    } catch (const std::exception& e) {
        std::cerr << "[DB] RecordValidation failed: " << e.what() << std::endl;
    }
}

std::vector<DatabaseManager::ValidationRule> DatabaseManager::GetRulesForService(
    const std::string& service_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<ValidationRule> rules;

    try {
        pqxx::work txn(*conn_);

        pqxx::result r =
            txn.exec_params("SELECT rule_id, service_name, field_path, rule_type, "
                            "       rule_config, COALESCE(error_message, '') as error_message "
                            "FROM validation_rules "
                            "WHERE service_name = $1 AND is_active = true "
                            "ORDER BY field_path",
                            service_name);

        txn.commit();

        for (const auto& row : r) {
            ValidationRule rule;
            rule.rule_id = row["rule_id"].as<std::string>();
            rule.service_name = row["service_name"].as<std::string>();
            rule.field_path = row["field_path"].as<std::string>();
            rule.rule_type = row["rule_type"].as<std::string>();
            rule.rule_config = row["rule_config"].as<std::string>();
            rule.error_message = row["error_message"].as<std::string>();
            rules.push_back(rule);
        }

        return rules;

    } catch (const std::exception& e) {
        std::cerr << "[DB] GetRulesForService failed: " << e.what() << std::endl;
        return rules;
    }
}

}  // namespace validationservice