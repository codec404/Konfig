#include "validation_service/config.h"

#include <iostream>

namespace validationservice {

ServiceConfig ServiceConfig::LoadFromFile(const std::string& path) {
    ServiceConfig config;

    try {
        YAML::Node yaml = YAML::LoadFile(path);

        if (yaml["server"]) {
            auto srv = yaml["server"];
            config.server.port = srv["port"].as<int>(8083);
            config.server.max_connections = srv["max_connections"].as<int>(500);
        }

        if (yaml["postgres"]) {
            auto pg = yaml["postgres"];
            config.postgres.host = pg["host"].as<std::string>("postgres");
            config.postgres.port = pg["port"].as<int>(5432);
            config.postgres.database = pg["database"].as<std::string>("configservice");
            config.postgres.user = pg["user"].as<std::string>("configuser");
            config.postgres.password = pg["password"].as<std::string>("configpass");
            config.postgres.max_connections = pg["max_connections"].as<int>(10);
        }

        if (yaml["redis"]) {
            auto redis = yaml["redis"];
            config.redis.host = redis["host"].as<std::string>("redis");
            config.redis.port = redis["port"].as<int>(6379);
            config.redis.cache_ttl_seconds = redis["cache_ttl"].as<int>(600);
        }

        if (yaml["statsd"]) {
            auto statsd = yaml["statsd"];
            config.statsd.host = statsd["host"].as<std::string>("statsd-exporter");
            config.statsd.port = statsd["port"].as<int>(9125);
            config.statsd.prefix = statsd["prefix"].as<std::string>("validation");
        }

        if (yaml["validation"]) {
            auto val = yaml["validation"];
            config.validation.max_config_size = val["max_config_size"].as<size_t>(1048576);
            config.validation.timeout_seconds = val["timeout_seconds"].as<int>(5);
            config.validation.enable_caching = val["enable_caching"].as<bool>(true);
            config.validation.strict_mode = val["strict_mode"].as<bool>(false);
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

}  // namespace validationservice