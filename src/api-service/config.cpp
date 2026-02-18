#include "api_service/config.h"

#include <iostream>

namespace apiservice {

ServiceConfig ServiceConfig::LoadFromFile(const std::string& path) {
    ServiceConfig config;

    try {
        YAML::Node yaml = YAML::LoadFile(path);

        if (yaml["server"]) {
            config.server.port = yaml["server"]["port"].as<int>(8081);
            config.server.max_connections = yaml["server"]["max_connections"].as<int>(1000);
        }

        if (yaml["postgres"]) {
            auto pg = yaml["postgres"];
            config.postgres.host = pg["host"].as<std::string>("postgres");
            config.postgres.port = pg["port"].as<int>(5432);
            config.postgres.database = pg["database"].as<std::string>("configservice");
            config.postgres.user = pg["user"].as<std::string>("configuser");
            config.postgres.password = pg["password"].as<std::string>("configpass");
            config.postgres.max_connections = pg["max_connections"].as<int>(25);
        }

        if (yaml["kafka"]) {
            config.kafka.brokers = yaml["kafka"]["brokers"].as<std::string>("kafka:9092");
            config.kafka.topic = yaml["kafka"]["topic"].as<std::string>("config.events");
        }

        if (yaml["redis"]) {
            config.redis.host = yaml["redis"]["host"].as<std::string>("redis");
            config.redis.port = yaml["redis"]["port"].as<int>(6379);
            config.redis.cache_ttl_seconds = yaml["redis"]["cache_ttl"].as<int>(300);
        }

        if (yaml["statsd"]) {
            config.statsd.host = yaml["statsd"]["host"].as<std::string>("statsd-exporter");
            config.statsd.port = yaml["statsd"]["port"].as<int>(9125);
            config.statsd.prefix = yaml["statsd"]["prefix"].as<std::string>("api");
        }

        if (yaml["validation_service"]) {
            config.validation.address =
                yaml["validation_service"]["address"].as<std::string>("validation-service:8083");
        }

        std::cout << "[Config] Loaded from: " << path << std::endl;

    } catch (const YAML::Exception& e) {
        std::cerr << "[Config] Error: " << e.what() << std::endl;
        std::cerr << "[Config] Using defaults" << std::endl;
        return LoadDefaults();
    }

    return config;
}

ServiceConfig ServiceConfig::LoadDefaults() {
    ServiceConfig config;
    std::cout << "[Config] Using default configuration" << std::endl;
    return config;
}

}  // namespace apiservice