#pragma once

#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace configservice {

struct ServerConfig {
    int port = 8082;
    int max_connections = 1000;
    int read_timeout_seconds = 60;
    int write_timeout_seconds = 60;
};

struct PostgresConfig {
    std::string host = "postgres";
    int port = 5432;
    std::string database = "configservice";
    std::string user = "configuser";
    std::string password = "configpass";
    int max_connections = 25;
    int connection_timeout_seconds = 10;
};

struct RedisConfig {
    std::string host = "redis";
    int port = 6379;
    int db = 0;
    int max_connections = 10;
    int connection_timeout_seconds = 5;
    int cache_ttl_seconds = 300;
};

struct KafkaConfig {
    std::vector<std::string> brokers = {"kafka:9092"};
    std::string topic = "config.updates";
    std::string compression = "gzip";
    int batch_size = 100;
};

struct StatsDConfig {
    std::string host = "statsd-exporter";
    int port = 9125;
    std::string prefix = "distribution";
    int flush_interval_seconds = 1;
};

struct MonitoringConfig {
    int heartbeat_interval_seconds = 30;
    int heartbeat_timeout_seconds = 90;
    int health_check_port = 8083;
};

struct LoggingConfig {
    std::string level = "info";
    std::string format = "json";
};

struct ServiceConfig {
    ServerConfig server;
    PostgresConfig postgres;
    RedisConfig redis;
    KafkaConfig kafka;
    StatsDConfig statsd;
    MonitoringConfig monitoring;
    LoggingConfig logging;

    static ServiceConfig LoadFromFile(const std::string& config_file);
    static ServiceConfig LoadDefaults();
};

}  // namespace configservice