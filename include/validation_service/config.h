#pragma once

#include <string>
#include <yaml-cpp/yaml.h>

namespace validationservice {

struct ServerConfig {
    int port = 8083;
    int max_connections = 500;
};

struct PostgresConfig {
    std::string host = "postgres";
    int port = 5432;
    std::string database = "configservice";
    std::string user = "configuser";
    std::string password = "configpass";
    int max_connections = 10;
    int connection_timeout_seconds = 10;
};

struct RedisConfig {
    std::string host = "redis";
    int port = 6379;
    int cache_ttl_seconds = 600;
};

struct StatsDConfig {
    std::string host = "statsd-exporter";
    int port = 9125;
    std::string prefix = "validation";
};

struct ValidationConfig {
    size_t max_config_size = 1024 * 1024;  // 1MB
    int timeout_seconds = 5;
    bool enable_caching = true;
    bool strict_mode = false;
};

struct ServiceConfig {
    ServerConfig server;
    PostgresConfig postgres;
    RedisConfig redis;
    StatsDConfig statsd;
    ValidationConfig validation;

    static ServiceConfig LoadFromFile(const std::string& path);
    static ServiceConfig LoadDefaults();
};

}  // namespace validationservice