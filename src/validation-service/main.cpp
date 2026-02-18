#include <grpcpp/grpcpp.h>

#include <csignal>
#include <iostream>
#include <memory>
#include <string>

#include "validation_service/config.h"
#include "validation_service/validation_service.h"

std::unique_ptr<grpc::Server> server;
std::unique_ptr<validationservice::ValidationServiceImpl> service;

void SignalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (service)
        service->Shutdown();
    if (server)
        server->Shutdown();
}

int main(int argc, char** argv) {
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "  Configuration Validation Service" << std::endl;
    std::cout << "  Version: 1.0.0" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << std::endl;

    // Load config
    std::string config_file = "config/validation-service.yml";
    if (argc > 1)
        config_file = argv[1];

    validationservice::ServiceConfig config;
    try {
        config = validationservice::ServiceConfig::LoadFromFile(config_file);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load config: " << e.what() << std::endl;
        config = validationservice::ServiceConfig::LoadDefaults();
    }

    // Create and initialize service
    service = std::make_unique<validationservice::ValidationServiceImpl>(config);
    if (!service->Initialize()) {
        std::cerr << "Failed to initialize service" << std::endl;
        return 1;
    }

    // Build gRPC server
    std::string server_address = "0.0.0.0:" + std::to_string(config.server.port);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(service.get());
    builder.SetMaxReceiveMessageSize(4 * 1024 * 1024);
    builder.SetMaxSendMessageSize(4 * 1024 * 1024);

    server = builder.BuildAndStart();

    if (!server) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "✓ Validation Service listening on " << server_address << std::endl;
    std::cout << "✓ Press Ctrl+C to stop" << std::endl;
    std::cout << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  PostgreSQL:    " << config.postgres.host << ":" << config.postgres.port
              << std::endl;
    std::cout << "  Redis:         " << config.redis.host << ":" << config.redis.port << std::endl;
    std::cout << "  StatsD:        " << config.statsd.host << ":" << config.statsd.port
              << std::endl;
    std::cout << "  Max Size:      " << config.validation.max_config_size << " bytes" << std::endl;
    std::cout << "  Caching:       " << (config.validation.enable_caching ? "enabled" : "disabled")
              << std::endl;
    std::cout << std::endl;

    server->Wait();

    std::cout << "Server stopped" << std::endl;
    return 0;
}