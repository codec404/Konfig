#include "distribution_service/event_publisher.h"

#include <ctime>
#include <iostream>
#include <sstream>

namespace configservice {

EventPublisher::EventPublisher(const KafkaConfig& config) : config_(config), initialized_(false) {}

EventPublisher::~EventPublisher() {
    Shutdown();
}

bool EventPublisher::Initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        std::string errstr;
        RdKafka::Conf* conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

        // Build broker list
        std::ostringstream brokers;
        for (size_t i = 0; i < config_.brokers.size(); ++i) {
            if (i > 0)
                brokers << ",";
            brokers << config_.brokers[i];
        }

        if (conf->set("bootstrap.servers", brokers.str(), errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "[Kafka] Config error: " << errstr << std::endl;
            delete conf;
            return false;
        }

        // Set compression
        if (conf->set("compression.type", config_.compression, errstr) != RdKafka::Conf::CONF_OK) {
            std::cerr << "[Kafka] Compression config error: " << errstr << std::endl;
        }

        // Create producer
        producer_.reset(RdKafka::Producer::create(conf, errstr));
        delete conf;

        if (!producer_) {
            std::cerr << "[Kafka] ✗ Failed to create producer: " << errstr << std::endl;
            return false;
        }

        std::cout << "[Kafka] ✓ Producer created" << std::endl;
        std::cout << "[Kafka]   Brokers: " << brokers.str() << std::endl;
        std::cout << "[Kafka]   Topic: " << config_.topic << std::endl;

        initialized_ = true;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[Kafka] ✗ Initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void EventPublisher::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (producer_) {
        // Wait for messages to be delivered
        producer_->flush(10000);  // 10 seconds timeout
        producer_.reset();
    }

    initialized_ = false;
    std::cout << "[Kafka] Producer shutdown" << std::endl;
}

std::string EventPublisher::BuildEventJson(const std::string& event_type,
                                           const std::string& service_name,
                                           const std::string& instance_id, int64_t version) {
    std::ostringstream json;
    json << "{"
         << "\"event_type\":\"" << event_type << "\","
         << "\"service_name\":\"" << service_name << "\","
         << "\"instance_id\":\"" << instance_id << "\"";

    if (version > 0) {
        json << ",\"version\":" << version;
    }

    json << ",\"timestamp\":" << std::time(nullptr) << "}";

    return json.str();
}

bool EventPublisher::Publish(const std::string& event_json) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !producer_) {
        return false;
    }

    RdKafka::ErrorCode err = producer_->produce(
        config_.topic, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY,
        const_cast<char*>(event_json.c_str()), event_json.size(), nullptr, 0, 0, nullptr);

    if (err != RdKafka::ERR_NO_ERROR) {
        std::cerr << "[Kafka] Produce failed: " << RdKafka::err2str(err) << std::endl;
        return false;
    }

    producer_->poll(0);  // Trigger callbacks
    return true;
}

bool EventPublisher::PublishConfigUpdate(const std::string& service_name,
                                         const std::string& instance_id, int64_t version) {
    std::string event = BuildEventJson("config_update", service_name, instance_id, version);

    if (Publish(event)) {
        std::cout << "[Kafka] Published: config_update for " << service_name << " v" << version
                  << std::endl;
        return true;
    }

    return false;
}

bool EventPublisher::PublishClientConnect(const std::string& service_name,
                                          const std::string& instance_id) {
    std::string event = BuildEventJson("client_connect", service_name, instance_id);

    if (Publish(event)) {
        std::cout << "[Kafka] Published: client_connect for " << instance_id << std::endl;
        return true;
    }

    return false;
}

bool EventPublisher::PublishClientDisconnect(const std::string& service_name,
                                             const std::string& instance_id) {
    std::string event = BuildEventJson("client_disconnect", service_name, instance_id);

    if (Publish(event)) {
        std::cout << "[Kafka] Published: client_disconnect for " << instance_id << std::endl;
        return true;
    }

    return false;
}

}  // namespace configservice