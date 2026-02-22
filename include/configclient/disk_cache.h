#pragma once

#include "config.pb.h"

#include <string>

namespace configservice {

/**
 * @brief Disk-based config cache for the Client SDK.
 *
 * Stores the last received ConfigData to disk using protobuf binary format.
 * On client startup, the cache is loaded and served immediately so the app
 * has a config even before the Distribution Service connection is established.
 *
 * Cache location: {cache_dir}/{service_name}.cache
 * Default dir:    ~/.konfig/cache/
 *
 * Writes are atomic (write to .tmp, then rename) to prevent corruption.
 * Loads verify the content_hash field to detect corruption.
 */
class DiskCache {
   public:
    /**
     * @param cache_dir Directory to store cache files. Empty string uses ~/.konfig/cache/.
     */
    explicit DiskCache(const std::string& cache_dir = "");

    /**
     * @brief Atomically save a config to disk.
     * @return true on success, false on I/O error.
     */
    bool Save(const ConfigData& config);

    /**
     * @brief Load cached config for a service.
     * @param service_name Service to look up.
     * @param out         Populated on success.
     * @return true if cache exists and passes integrity check.
     */
    bool Load(const std::string& service_name, ConfigData& out);

    /**
     * @brief Check whether a cache file exists for the service.
     */
    bool Exists(const std::string& service_name) const;

    /**
     * @brief Full path to the cache file for a service.
     */
    std::string GetCachePath(const std::string& service_name) const;

   private:
    std::string cache_dir_;

    bool EnsureCacheDir() const;
    static std::string ComputeHash(const std::string& content);
    static std::string ResolveDefaultCacheDir();
};

}  // namespace configservice
