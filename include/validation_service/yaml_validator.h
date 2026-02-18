#pragma once

#include <string>
#include <vector>

#include "validation.pb.h"

namespace validationservice {

class YamlValidator {
   public:
    YamlValidator() = default;

    // Validate YAML syntax
    bool ValidateSyntax(const std::string& content,
                        std::vector<configservice::ValidationError>& errors);

    // Validate YAML structure
    bool ValidateStructure(const std::string& content,
                           std::vector<configservice::ValidationError>& errors,
                           std::vector<configservice::ValidationWarning>& warnings);

    // Check for common YAML issues
    bool CheckCommonIssues(const std::string& content,
                           std::vector<configservice::ValidationWarning>& warnings);

   private:
    void AddError(std::vector<configservice::ValidationError>& errors, const std::string& field,
                  const std::string& type, const std::string& message, int line = 0);

    void AddWarning(std::vector<configservice::ValidationWarning>& warnings,
                    const std::string& field, const std::string& type, const std::string& message);
};

}  // namespace validationservice