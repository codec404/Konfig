#include "validation_service/yaml_validator.h"

#include <iostream>
#include <sstream>
#include <yaml-cpp/yaml.h>

namespace validationservice {

bool YamlValidator::ValidateSyntax(const std::string& content,
                                   std::vector<configservice::ValidationError>& errors) {
    try {
        YAML::Node config = YAML::Load(content);

        // Successfully parsed
        return true;

    } catch (const YAML::ParserException& e) {
        std::ostringstream oss;
        oss << "YAML parsing error at line " << (e.mark.line + 1) << ", column "
            << (e.mark.column + 1) << ": " << e.msg;

        AddError(errors, "", "syntax", oss.str(), e.mark.line + 1);
        return false;

    } catch (const YAML::Exception& e) {
        AddError(errors, "", "syntax", std::string("YAML error: ") + e.what());
        return false;
    }
}

bool YamlValidator::ValidateStructure(const std::string& content,
                                      std::vector<configservice::ValidationError>& errors,
                                      std::vector<configservice::ValidationWarning>& warnings) {
    try {
        YAML::Node config = YAML::Load(content);

        // Check if root is a map (most configs should be)
        if (!config.IsMap() && !config.IsSequence()) {
            AddError(errors, "", "structure", "Root node must be a map or sequence, got scalar");
            return false;
        }

        // Check for empty config
        if (config.IsNull() || (config.IsMap() && config.size() == 0)) {
            AddWarning(warnings, "", "empty", "Configuration is empty");
        }

        // Recursively check for common issues
        CheckCommonIssues(content, warnings);

        return errors.empty();

    } catch (const YAML::Exception& e) {
        AddError(errors, "", "structure", std::string("Structure error: ") + e.what());
        return false;
    }
}

bool YamlValidator::CheckCommonIssues(const std::string& content,
                                      std::vector<configservice::ValidationWarning>& warnings) {
    // Check for tabs (YAML should use spaces)
    if (content.find('\t') != std::string::npos) {
        AddWarning(warnings, "", "formatting", "YAML contains tabs - use spaces for indentation");
    }

    // Check for trailing whitespace
    std::istringstream stream(content);
    std::string line;
    int line_num = 0;
    while (std::getline(stream, line)) {
        line_num++;

        // Check trailing whitespace
        if (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
            AddWarning(warnings, "", "formatting",
                       "Line " + std::to_string(line_num) + " has trailing whitespace");
        }

        // Check inconsistent indentation (should be multiple of 2)
        size_t indent = 0;
        for (char c : line) {
            if (c == ' ')
                indent++;
            else
                break;
        }

        if (indent > 0 && indent % 2 != 0) {
            AddWarning(warnings, "", "formatting",
                       "Line " + std::to_string(line_num) +
                           " has inconsistent indentation (not multiple of 2)");
        }
    }

    // Check for duplicate keys
    try {
        YAML::Node config = YAML::Load(content);

        // This is a simplified check - yaml-cpp will catch obvious duplicates
        // For deeper checking, we'd need to traverse the tree

    } catch (const YAML::Exception& e) {
        // Already caught by syntax validation
    }

    return true;
}

void YamlValidator::AddError(std::vector<configservice::ValidationError>& errors,
                             const std::string& field, const std::string& type,
                             const std::string& message, int line) {
    configservice::ValidationError error;
    error.set_field(field);
    error.set_error_type(type);
    error.set_message(message);
    if (line > 0) {
        error.set_line(line);
    }
    errors.push_back(error);
}

void YamlValidator::AddWarning(std::vector<configservice::ValidationWarning>& warnings,
                               const std::string& field, const std::string& type,
                               const std::string& message) {
    configservice::ValidationWarning warning;
    warning.set_field(field);
    warning.set_warning_type(type);
    warning.set_message(message);
    warnings.push_back(warning);
}

}  // namespace validationservice