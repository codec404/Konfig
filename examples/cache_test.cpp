#include "configclient/config_client.h"
#include "configclient/disk_cache.h"

#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

using namespace configservice;

std::atomic<bool> keep_running(true);

void signal_handler(int) {
    keep_running = false;
}

static void print_separator() {
    std::cout << std::string(50, '-') << std::endl;
}

int main(int argc, char** argv) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::string server_address = "localhost:8082";
    std::string service_name = "payment-service";
    std::string cache_dir = "";  // defaults to ~/.konfig/cache/

    if (argc > 1)
        server_address = argv[1];
    if (argc > 2)
        service_name = argv[2];
    if (argc > 3)
        cache_dir = argv[3];

    std::cout << "[CacheTest] server  : " << server_address << std::endl;
    std::cout << "[CacheTest] service : " << service_name << std::endl;
    print_separator();

    // ----------------------------------------------------------------
    // Step 1: show what is already on disk BEFORE constructing the client
    // ----------------------------------------------------------------
    {
        DiskCache probe(cache_dir);
        if (probe.Exists(service_name)) {
            std::cout << "[CacheTest] Cache file exists  : " << probe.GetCachePath(service_name)
                      << std::endl;
            ConfigData tmp;
            if (probe.Load(service_name, tmp)) {
                std::cout << "[CacheTest] Cache readable     : YES  (v" << tmp.version() << ")"
                          << std::endl;
            } else {
                std::cout << "[CacheTest] Cache readable     : NO   (corrupt — will be discarded)"
                          << std::endl;
            }
        } else {
            std::cout << "[CacheTest] Cache file exists  : NO  (first run)" << std::endl;
        }
        print_separator();
    }

    // ----------------------------------------------------------------
    // Step 2: create client — Start() loads cache and fires callback
    // ----------------------------------------------------------------
    ConfigClient client(server_address, service_name, /*instance_id=*/"", cache_dir);

    client.OnConfigUpdate([](const ConfigData& config) {
        std::cout << "\n>>> CONFIG UPDATE <<<" << std::endl;
        std::cout << "  config_id : " << config.config_id() << std::endl;
        std::cout << "  version   : " << config.version() << std::endl;
        std::cout << "  format    : " << config.format() << std::endl;
        std::cout << "  content   : "
                  << config.content().substr(0, std::min<size_t>(120, config.content().size()))
                  << (config.content().size() > 120 ? "..." : "") << std::endl;
        std::cout << ">>>" << std::endl;
    });

    client.OnConnectionStatus([](bool connected) {
        if (connected) {
            std::cout << "[Status] Connected to distribution service" << std::endl;
        } else {
            std::cout << "[Status] Disconnected from distribution service" << std::endl;
        }
    });

    if (!client.Start()) {
        std::cerr << "[CacheTest] Failed to start client" << std::endl;
        return 1;
    }

    std::cout << "[CacheTest] Running — Ctrl+C to stop" << std::endl;
    print_separator();

    // ----------------------------------------------------------------
    // Step 3: periodic status line so it's clear the client is alive
    // ----------------------------------------------------------------
    int tick = 0;
    while (keep_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (++tick % 10 == 0) {
            std::cout << "[CacheTest] alive  connected=" << client.IsConnected()
                      << "  version=" << client.GetCurrentVersion() << std::endl;
        }
    }

    std::cout << std::endl;
    client.Stop();
    std::cout << "[CacheTest] Done" << std::endl;
    return 0;
}
