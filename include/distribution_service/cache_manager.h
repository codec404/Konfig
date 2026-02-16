#pragma once

#include "config.h"
#include "config.pb.h"

#include <hiredis/hiredis.h>
#include <memory>
#include <mutex>
#include <string>

namespace configservice {

class CacheManager {
   public:
    explicit CacheManager(const RedisConfig& config);
    ~CacheManager();

    bool Initialize();
    void Shutdown();

    // Cache operations
    bool Set(const std::string& key, const std::string& value, int ttl_seconds = 0);
    std::string Get(const std::string& key);
    bool Delete(const std::string& key);
    bool Exists(const std::string& key);

    // Config-specific operations
    bool CacheConfig(const ConfigData& config);
    ConfigData GetCachedConfig(const std::string& service_name, int64_t version);
    std::string BuildConfigCacheKey(const std::string& service_name, int64_t version);

    // Stats
    int64_t IncrementCounter(const std::string& key);
    void SetGauge(const std::string& key, int64_t value);

   private:
    RedisConfig config_;
    redisContext* context_;
    std::mutex mutex_;
    bool initialized_;

    bool Reconnect();
};

}  // namespace configservice