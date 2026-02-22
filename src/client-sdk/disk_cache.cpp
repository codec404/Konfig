#include "configclient/disk_cache.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

// SHA-256 via OpenSSL (already a transitive dependency of gRPC)
#include <openssl/sha.h>

namespace configservice {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

DiskCache::DiskCache(const std::string& cache_dir)
    : cache_dir_(cache_dir.empty() ? ResolveDefaultCacheDir() : cache_dir) {}

std::string DiskCache::ResolveDefaultCacheDir() {
    const char* home = std::getenv("HOME");
    if (home && *home) {
        return std::string(home) + "/.konfig/cache";
    }
    // Fallback: current directory
    return ".konfig/cache";
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::string DiskCache::GetCachePath(const std::string& service_name) const {
    // Sanitise service name: replace '/' and '\' with '_'
    std::string safe = service_name;
    for (char& c : safe) {
        if (c == '/' || c == '\\')
            c = '_';
    }
    return cache_dir_ + "/" + safe + ".cache";
}

bool DiskCache::Exists(const std::string& service_name) const {
    struct stat st{};
    return stat(GetCachePath(service_name).c_str(), &st) == 0;
}

bool DiskCache::Save(const ConfigData& config) {
    if (!EnsureCacheDir()) {
        std::cerr << "[DiskCache] Cannot create cache directory: " << cache_dir_ << std::endl;
        return false;
    }

    std::string data;
    if (!config.SerializeToString(&data)) {
        std::cerr << "[DiskCache] Serialization failed for " << config.service_name() << std::endl;
        return false;
    }

    std::string path = GetCachePath(config.service_name());
    std::string tmp_path = path + ".tmp";

    // Write to temp file first
    {
        std::ofstream f(tmp_path, std::ios::binary | std::ios::trunc);
        if (!f) {
            std::cerr << "[DiskCache] Cannot write to " << tmp_path << std::endl;
            return false;
        }
        f.write(data.data(), static_cast<std::streamsize>(data.size()));
    }

    // Atomic rename
    if (std::rename(tmp_path.c_str(), path.c_str()) != 0) {
        std::cerr << "[DiskCache] Rename failed: " << tmp_path << " -> " << path << std::endl;
        std::remove(tmp_path.c_str());
        return false;
    }

    std::cout << "[DiskCache] Saved config v" << config.version() << " for "
              << config.service_name() << " -> " << path << std::endl;
    return true;
}

bool DiskCache::Load(const std::string& service_name, ConfigData& out) {
    std::string path = GetCachePath(service_name);

    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;  // Cache miss — not an error
    }

    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    if (!out.ParseFromString(data)) {
        std::cerr << "[DiskCache] Parse failed for " << path << " — discarding" << std::endl;
        std::remove(path.c_str());
        return false;
    }

    // Verify content integrity using stored hash
    if (!out.content_hash().empty()) {
        std::string actual_hash = ComputeHash(out.content());
        if (actual_hash != out.content_hash()) {
            std::cerr << "[DiskCache] Hash mismatch for " << service_name << " — discarding"
                      << std::endl;
            std::remove(path.c_str());
            return false;
        }
    }

    std::cout << "[DiskCache] Loaded cached config v" << out.version() << " for " << service_name
              << std::endl;
    return true;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool DiskCache::EnsureCacheDir() const {
    struct stat st{};
    if (stat(cache_dir_.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }

    // mkdir -p: create each component
    std::string path;
    for (size_t i = 0; i <= cache_dir_.size(); ++i) {
        if (i == cache_dir_.size() || cache_dir_[i] == '/') {
            if (!path.empty()) {
                struct stat s{};
                if (stat(path.c_str(), &s) != 0) {
#ifdef _WIN32
                    if (mkdir(path.c_str()) != 0)
                        return false;
#else
                    if (mkdir(path.c_str(), 0755) != 0)
                        return false;
#endif
                }
            }
        }
        if (i < cache_dir_.size())
            path += cache_dir_[i];
    }
    return true;
}

std::string DiskCache::ComputeHash(const std::string& content) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(content.data()), content.size(), hash);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned char byte : hash) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

}  // namespace configservice
