#pragma once

#include "config.h"

#include <librdkafka/rdkafkacpp.h>
#include <memory>
#include <mutex>
#include <string>

namespace configservice {

class EventPublisher {
   public:
    explicit EventPublisher(const KafkaConfig& config);
    ~EventPublisher();

    bool Initialize();
    void Shutdown();

    // Publish events
    bool PublishConfigUpdate(const std::string& service_name, const std::string& instance_id,
                             int64_t version);

    bool PublishClientConnect(const std::string& service_name, const std::string& instance_id);

    bool PublishClientDisconnect(const std::string& service_name, const std::string& instance_id);

    // Generic publish
    bool Publish(const std::string& event_json);

   private:
    KafkaConfig config_;
    std::unique_ptr<RdKafka::Producer> producer_;
    std::mutex mutex_;
    bool initialized_;

    std::string BuildEventJson(const std::string& event_type, const std::string& service_name,
                               const std::string& instance_id, int64_t version = 0);
};

}  // namespace configservice