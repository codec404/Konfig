#include "distribution_service/config.h"

#include <fstream>
#include <iostream>

namespace configservice {

ServiceConfig ServiceConfig::LoadFromFile(const std::string& config_file) {
    ServiceConfig config;

    try {
        YAML::Node yaml = YAML::LoadFile(config_file);

        // Server
        if (yaml["server"]) {
            auto server = yaml["server"];
            config.server.port = server["port"].as<int>(8082);
            config.server.max_connections = server["max_connections"].as<int>(1000);
        }

        // PostgreSQL
        if (yaml["postgres"]) {
            auto pg = yaml["postgres"];
            config.postgres.host = pg["host"].as<std::string>("postgres");
            config.postgres.port = pg["port"].as<int>(5432);
            config.postgres.database = pg["database"].as<std::string>("configservice");
            config.postgres.user = pg["user"].as<std::string>("configuser");
            config.postgres.password = pg["password"].as<std::string>("configpass");
            config.postgres.max_connections = pg["max_connections"].as<int>(25);
        }

        // Redis
        if (yaml["redis"]) {
            auto redis = yaml["redis"];
            config.redis.host = redis["host"].as<std::string>("redis");
            config.redis.port = redis["port"].as<int>(6379);
            config.redis.db = redis["db"].as<int>(0);
            config.redis.cache_ttl_seconds = redis["cache_ttl"].as<int>(300);
        }

        // Kafka
        if (yaml["kafka"]) {
            auto kafka = yaml["kafka"];
            if (kafka["brokers"]) {
                config.kafka.brokers.clear();
                for (const auto& broker : kafka["brokers"]) {
                    config.kafka.brokers.push_back(broker.as<std::string>());
                }
            }
            config.kafka.topic = kafka["topic"].as<std::string>("config.updates");
        }

        // StatsD
        if (yaml["statsd"]) {
            auto statsd = yaml["statsd"];
            config.statsd.host = statsd["host"].as<std::string>("statsd-exporter");
            config.statsd.port = statsd["port"].as<int>(9125);
            config.statsd.prefix = statsd["prefix"].as<std::string>("distribution");
        }

        // Monitoring
        if (yaml["monitoring"]) {
            auto mon = yaml["monitoring"];
            config.monitoring.heartbeat_interval_seconds =
                mon["heartbeat_interval"].as<std::string>("30s")[0] - '0';
            config.monitoring.heartbeat_timeout_seconds =
                mon["heartbeat_timeout"].as<std::string>("90s")[0] - '0';
        }

        // Logging
        if (yaml["logging"]) {
            auto log = yaml["logging"];
            config.logging.level = log["level"].as<std::string>("info");
            config.logging.format = log["format"].as<std::string>("json");
        }

        std::cout << "[Config] Loaded from: " << config_file << std::endl;

    } catch (const YAML::Exception& e) {
        std::cerr << "[Config] YAML error: " << e.what() << std::endl;
        std::cerr << "[Config] Using default configuration" << std::endl;
        return LoadDefaults();
    }

    return config;
}

ServiceConfig ServiceConfig::LoadDefaults() {
    ServiceConfig config;
    std::cout << "[Config] Using default configuration" << std::endl;
    return config;
}

}  // namespace configservice