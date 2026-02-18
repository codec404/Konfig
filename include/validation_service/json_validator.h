#pragma once

#include <string>
#include <vector>

#include "validation.pb.h"

namespace validationservice {

class JsonValidator {
   public:
    JsonValidator() = default;

    // Validate JSON syntax
    bool ValidateSyntax(const std::string& content,
                        std::vector<configservice::ValidationError>& errors);

    // Validate against JSON schema
    bool ValidateSchema(const std::string& content, const std::string& schema,
                        std::vector<configservice::ValidationError>& errors);

    // Validate value ranges
    bool ValidateRanges(const std::string& content, const std::string& service_name,
                        std::vector<configservice::ValidationError>& errors);

    // Check required fields
    bool ValidateRequired(const std::string& content,
                          const std::vector<std::string>& required_fields,
                          std::vector<configservice::ValidationError>& errors);

   private:
    void AddError(std::vector<configservice::ValidationError>& errors, const std::string& field,
                  const std::string& type, const std::string& message);
};

}  // namespace validationservice