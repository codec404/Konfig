#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>

#include "configclient/config_client.h"

using namespace configservice;

std::atomic<bool> keep_running(true);

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    keep_running = false;
}

int main(int argc, char** argv) {
    // Setup signal handling
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::string server_address = "localhost:8082";
    std::string service_name = "example-service";
    
    if (argc > 1) {
        server_address = argv[1];
    }
    if (argc > 2) {
        service_name = argv[2];
    }

    std::cout << "=== Config Client Example ===" << std::endl;
    std::cout << "Service: " << service_name << std::endl;
    std::cout << "Server: " << server_address << std::endl;
    std::cout << "==============================" << std::endl << std::endl;

    ConfigClient client(server_address, service_name);

    // Register config update callback
    client.OnConfigUpdate([](const ConfigData& config) {
        std::cout << "\n>>> CONFIG UPDATE <<<" << std::endl;
        std::cout << "Config ID: " << config.config_id() << std::endl;
        std::cout << "Version: " << config.version() << std::endl;
        std::cout << "Format: " << config.format() << std::endl;
        std::cout << "Content length: " << config.content().length() << " bytes" << std::endl;
        std::cout << "Content preview: " 
                  << config.content().substr(0, std::min(size_t(100), config.content().length())) 
                  << (config.content().length() > 100 ? "..." : "") << std::endl;
        std::cout << ">>>" << std::endl << std::endl;
    });

    // Register connection callback
    client.OnConnectionStatus([](bool connected) {
        if (connected) {
            std::cout << "[Status] ✓ Connected to distribution service" << std::endl;
        } else {
            std::cout << "[Status] ✗ Disconnected from distribution service" << std::endl;
        }
    });

    // Start client
    if (!client.Start()) {
        std::cerr << "Failed to start client!" << std::endl;
        return 1;
    }

    std::cout << "Client started. Press Ctrl+C to exit." << std::endl;
    std::cout << "Waiting for configuration updates..." << std::endl << std::endl;

    // Main loop
    while (keep_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Cleanup
    std::cout << "\nStopping client..." << std::endl;
    client.Stop();
    std::cout << "Goodbye!" << std::endl;

    return 0;
}