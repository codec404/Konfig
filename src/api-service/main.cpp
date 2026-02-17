#include <grpcpp/grpcpp.h>

#include <csignal>
#include <iostream>
#include <memory>
#include <string>

#include "api_service/api_service.h"
#include "api_service/config.h"

std::unique_ptr<grpc::Server> server;
std::unique_ptr<apiservice::ApiServiceImpl> service;

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
    std::cout << "  Configuration API Service" << std::endl;
    std::cout << "  Version: 1.0.0" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << std::endl;

    // Load config
    std::string config_file = "config/api-service.yml";
    if (argc > 1)
        config_file = argv[1];

    apiservice::ServiceConfig config;
    try {
        config = apiservice::ServiceConfig::LoadFromFile(config_file);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load config: " << e.what() << std::endl;
        config = apiservice::ServiceConfig::LoadDefaults();
    }

    // Create and initialize service
    service = std::make_unique<apiservice::ApiServiceImpl>(config);
    if (!service->Initialize()) {
        std::cerr << "Failed to initialize service" << std::endl;
        return 1;
    }

    // Build gRPC server
    std::string server_address = "0.0.0.0:" + std::to_string(config.server.port);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(service.get());
    builder.SetMaxReceiveMessageSize(4 * 1024 * 1024);  // 4MB
    builder.SetMaxSendMessageSize(4 * 1024 * 1024);     // 4MB

    server = builder.BuildAndStart();

    if (!server) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "✓ API Service listening on " << server_address << std::endl;
    std::cout << "✓ Press Ctrl+C to stop" << std::endl;
    std::cout << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  PostgreSQL: " << config.postgres.host << ":" << config.postgres.port
              << std::endl;
    std::cout << "  Kafka:      " << config.kafka.brokers << std::endl;
    std::cout << "  StatsD:     " << config.statsd.host << ":" << config.statsd.port << std::endl;
    std::cout << std::endl;

    server->Wait();

    std::cout << "Server stopped" << std::endl;
    return 0;
}