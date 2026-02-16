#include "distribution_service/cache_manager.h"

#include <iostream>
#include <sstream>

namespace configservice {

CacheManager::CacheManager(const RedisConfig& config)
    : config_(config), context_(nullptr), initialized_(false) {}

CacheManager::~CacheManager() {
    Shutdown();
}

bool CacheManager::Initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        struct timeval timeout = {config_.connection_timeout_seconds, 0};

        context_ = redisConnectWithTimeout(config_.host.c_str(), config_.port, timeout);

        if (context_ == nullptr || context_->err) {
            if (context_) {
                std::cerr << "[Cache] ✗ Connection failed: " << context_->errstr << std::endl;
                redisFree(context_);
                context_ = nullptr;
            } else {
                std::cerr << "[Cache] ✗ Connection failed: Cannot allocate context" << std::endl;
            }
            return false;
        }

        // Test connection
        redisReply* reply = (redisReply*)redisCommand(context_, "PING");
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            std::cerr << "[Cache] ✗ PING failed" << std::endl;
            if (reply)
                freeReplyObject(reply);
            return false;
        }
        freeReplyObject(reply);

        std::cout << "[Cache] ✓ Connected to Redis" << std::endl;
        std::cout << "[Cache]   Host: " << config_.host << ":" << config_.port << std::endl;

        initialized_ = true;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[Cache] ✗ Initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void CacheManager::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (context_) {
        redisFree(context_);
        context_ = nullptr;
    }

    initialized_ = false;
    std::cout << "[Cache] Connection closed" << std::endl;
}

bool CacheManager::Reconnect() {
    Shutdown();
    return Initialize();
}

bool CacheManager::Set(const std::string& key, const std::string& value, int ttl_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !context_) {
        return false;
    }

    redisReply* reply;

    if (ttl_seconds > 0) {
        reply = (redisReply*)redisCommand(context_, "SETEX %s %d %b", key.c_str(), ttl_seconds,
                                          value.c_str(), value.size());
    } else {
        reply = (redisReply*)redisCommand(context_, "SET %s %b", key.c_str(), value.c_str(),
                                          value.size());
    }

    if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
        if (reply) {
            std::cerr << "[Cache] SET failed: " << reply->str << std::endl;
            freeReplyObject(reply);
        }
        return false;
    }

    freeReplyObject(reply);
    return true;
}

std::string CacheManager::Get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !context_) {
        return "";
    }

    redisReply* reply = (redisReply*)redisCommand(context_, "GET %s", key.c_str());

    if (reply == nullptr || reply->type != REDIS_REPLY_STRING) {
        if (reply)
            freeReplyObject(reply);
        return "";
    }

    std::string value(reply->str, reply->len);
    freeReplyObject(reply);

    return value;
}

bool CacheManager::Delete(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !context_) {
        return false;
    }

    redisReply* reply = (redisReply*)redisCommand(context_, "DEL %s", key.c_str());

    if (reply == nullptr) {
        return false;
    }

    bool success = reply->type == REDIS_REPLY_INTEGER && reply->integer > 0;
    freeReplyObject(reply);

    return success;
}

bool CacheManager::Exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !context_) {
        return false;
    }

    redisReply* reply = (redisReply*)redisCommand(context_, "EXISTS %s", key.c_str());

    if (reply == nullptr) {
        return false;
    }

    bool exists = reply->type == REDIS_REPLY_INTEGER && reply->integer > 0;
    freeReplyObject(reply);

    return exists;
}

std::string CacheManager::BuildConfigCacheKey(const std::string& service_name, int64_t version) {
    if (version <= 0) {
        return "config:latest:" + service_name;
    }
    return "config:" + service_name + ":v" + std::to_string(version);
}

bool CacheManager::CacheConfig(const ConfigData& config) {
    std::string key = BuildConfigCacheKey(config.service_name(), config.version());
    std::string value = config.SerializeAsString();

    bool success = Set(key, value, config_.cache_ttl_seconds);

    if (success) {
        std::cout << "[Cache] Cached config: " << key << std::endl;
    }

    return success;
}

ConfigData CacheManager::GetCachedConfig(const std::string& service_name, int64_t version) {
    std::string key = BuildConfigCacheKey(service_name, version);
    std::string value = Get(key);

    ConfigData config;

    if (!value.empty()) {
        if (config.ParseFromString(value)) {
            std::cout << "[Cache] Cache hit: " << key << std::endl;
            return config;
        }
    }

    std::cout << "[Cache] Cache miss: " << key << std::endl;
    return ConfigData();  // Empty config
}

int64_t CacheManager::IncrementCounter(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !context_) {
        return 0;
    }

    redisReply* reply = (redisReply*)redisCommand(context_, "INCR %s", key.c_str());

    if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) {
        if (reply)
            freeReplyObject(reply);
        return 0;
    }

    int64_t value = reply->integer;
    freeReplyObject(reply);

    return value;
}

void CacheManager::SetGauge(const std::string& key, int64_t value) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !context_) {
        return;
    }

    redisReply* reply = (redisReply*)redisCommand(context_, "SET %s %lld", key.c_str(), value);
    if (reply) {
        freeReplyObject(reply);
    }
}

}  // namespace configservice