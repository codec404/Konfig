#include "validation_service/json_validator.h"

#include <iostream>
#include <sstream>

namespace validationservice {

bool JsonValidator::ValidateSyntax(const std::string& content,
                                   std::vector<configservice::ValidationError>& errors) {
    // Basic JSON syntax validation
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    int line = 1;
    int column = 0;

    for (size_t i = 0; i < content.size(); ++i) {
        char c = content[i];
        column++;

        if (c == '\n') {
            line++;
            column = 0;
            continue;
        }

        if (escaped) {
            escaped = false;
            continue;
        }

        if (c == '\\' && in_string) {
            escaped = true;
            continue;
        }

        if (c == '"') {
            in_string = !in_string;
            continue;
        }

        if (!in_string) {
            if (c == '{' || c == '[') {
                depth++;
            } else if (c == '}' || c == ']') {
                // Check for trailing comma before closing bracket
                for (size_t j = i; j > 0; --j) {
                    char p = content[j - 1];
                    if (p == ' ' || p == '\t' || p == '\n' || p == '\r')
                        continue;
                    if (p == ',') {
                        AddError(errors, "", "syntax",
                                 "Trailing comma before '" + std::string(1, c) + "' at line " +
                                     std::to_string(line) + ", column " + std::to_string(column));
                        return false;
                    }
                    break;
                }
                depth--;
                if (depth < 0) {
                    AddError(errors, "", "syntax",
                             "Unexpected closing bracket at line " + std::to_string(line) +
                                 ", column " + std::to_string(column));
                    return false;
                }
            }
        }
    }

    if (depth != 0) {
        AddError(errors, "", "syntax", "Unclosed brackets (depth: " + std::to_string(depth) + ")");
        return false;
    }

    if (in_string) {
        AddError(errors, "", "syntax", "Unclosed string");
        return false;
    }

    return true;
}

bool JsonValidator::ValidateSchema(const std::string& content, const std::string& schema,
                                   std::vector<configservice::ValidationError>& errors) {
    // TODO: Implement JSON Schema validation
    // For now, return true (would use a library like rapidjson-schema)
    return true;
}

bool JsonValidator::ValidateRanges(const std::string& content, const std::string& service_name,
                                   std::vector<configservice::ValidationError>& errors) {
    // Basic range validation
    // In a real implementation, parse JSON and check specific fields

    // Example: Check if max_connections is in valid range
    if (content.find("\"max_connections\"") != std::string::npos) {
        // Extract value (simplified - would use proper JSON parsing)
        size_t pos = content.find("\"max_connections\"");
        size_t colon = content.find(":", pos);
        size_t comma = content.find_first_of(",}", colon);

        if (colon != std::string::npos && comma != std::string::npos) {
            std::string value_str = content.substr(colon + 1, comma - colon - 1);

            // Trim whitespace
            value_str.erase(0, value_str.find_first_not_of(" \t\n\r"));
            value_str.erase(value_str.find_last_not_of(" \t\n\r") + 1);

            try {
                int value = std::stoi(value_str);
                if (value < 1 || value > 1000) {
                    AddError(
                        errors, "max_connections", "range",
                        "max_connections must be between 1 and 1000, got " + std::to_string(value));
                    return false;
                }
            } catch (...) {
                // Invalid number format
            }
        }
    }

    return errors.empty();
}

bool JsonValidator::ValidateRequired(const std::string& content,
                                     const std::vector<std::string>& required_fields,
                                     std::vector<configservice::ValidationError>& errors) {
    bool valid = true;

    for (const auto& field : required_fields) {
        std::string search = "\"" + field + "\"";
        if (content.find(search) == std::string::npos) {
            AddError(errors, field, "required", "Required field '" + field + "' is missing");
            valid = false;
        }
    }

    return valid;
}

void JsonValidator::AddError(std::vector<configservice::ValidationError>& errors,
                             const std::string& field, const std::string& type,
                             const std::string& message) {
    configservice::ValidationError error;
    error.set_field(field);
    error.set_error_type(type);
    error.set_message(message);
    errors.push_back(error);
}

}  // namespace validationservice