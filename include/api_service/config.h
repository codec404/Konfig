#pragma once

#include <string>
#include <yaml-cpp/yaml.h>

namespace apiservice {

struct PostgresConfig {
    std::string host = "postgres";
    int port = 5432;
    std::string database = "configservice";
    std::string user = "configuser";
    std::string password = "configpass";
    int max_connections = 25;
    int connection_timeout_seconds = 10;
};

struct KafkaConfig {
    std::string brokers = "kafka:9092";
    std::string topic = "config.events";
};

struct RedisConfig {
    std::string host = "redis";
    int port = 6379;
    int cache_ttl_seconds = 300;
};

struct StatsDConfig {
    std::string host = "statsd-exporter";
    int port = 9125;
    std::string prefix = "api";
};

struct ServerConfig {
    int port = 8081;
    int max_connections = 1000;
};

struct ValidationServiceConfig {
    std::string address = "validation-service:8083";
};

struct ServiceConfig {
    ServerConfig server;
    PostgresConfig postgres;
    KafkaConfig kafka;
    RedisConfig redis;
    StatsDConfig statsd;
    ValidationServiceConfig validation;

    static ServiceConfig LoadFromFile(const std::string& path);
    static ServiceConfig LoadDefaults();
};

}  // namespace apiservice