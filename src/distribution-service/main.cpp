#include <grpcpp/grpcpp.h>

#include <csignal>
#include <iostream>
#include <memory>
#include <string>

#include "distribution_service/config.h"
#include "distribution_service/distribution_service.h"

std::unique_ptr<grpc::Server> server;
std::unique_ptr<configservice::DistributionServiceImpl> service;

void SignalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;

    if (service) {
        service->Shutdown();
    }

    if (server) {
        server->Shutdown();
    }
}

int main(int argc, char** argv) {
    // Setup signal handlers
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "  Configuration Distribution Service" << std::endl;
    std::cout << "  Version: 1.0.0" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << std::endl;

    // Load configuration
    std::string config_file = "config/distribution-service.yml";
    if (argc > 1) {
        config_file = argv[1];
    }

    configservice::ServiceConfig config;
    try {
        config = configservice::ServiceConfig::LoadFromFile(config_file);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load config: " << e.what() << std::endl;
        config = configservice::ServiceConfig::LoadDefaults();
    }

    // Create and initialize service
    service = std::make_unique<configservice::DistributionServiceImpl>(config);

    if (!service->Initialize()) {
        std::cerr << "Failed to initialize service" << std::endl;
        return 1;
    }

    // Build gRPC server
    std::string server_address = "0.0.0.0:" + std::to_string(config.server.port);

    grpc::ServerBuilder builder;

    // Add listening port
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // Register service
    builder.RegisterService(service.get());

    // Set server options
    builder.SetMaxReceiveMessageSize(4 * 1024 * 1024);  // 4MB
    builder.SetMaxSendMessageSize(4 * 1024 * 1024);     // 4MB
    builder.SetMaxMessageSize(4 * 1024 * 1024);         // 4MB

    // Build and start server
    server = builder.BuildAndStart();

    if (!server) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "✓ Server listening on " << server_address << std::endl;
    std::cout << "✓ Press Ctrl+C to stop" << std::endl;
    std::cout << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  PostgreSQL: " << config.postgres.host << ":" << config.postgres.port
              << std::endl;
    std::cout << "  Redis:      " << config.redis.host << ":" << config.redis.port << std::endl;
    std::cout << "  Kafka:      " << config.kafka.brokers[0] << std::endl;
    std::cout << "  StatsD:     " << config.statsd.host << ":" << config.statsd.port << std::endl;
    std::cout << std::endl;

    // Wait for server to shutdown
    server->Wait();

    std::cout << "Server stopped" << std::endl;
    return 0;
}