#ifndef FLASH_MODEL_VALIDATOR_HPP
#define FLASH_MODEL_VALIDATOR_HPP

#include "flash_model/config.hpp"

#include <string>
#include <vector>

namespace flash_model {

enum class ValidationLevel {
    EvidenceGap,
    Warning,
    Error
};

struct ValidationMessage {
    ValidationLevel level = ValidationLevel::Warning;
    std::string text;
};

struct ValidationReport {
    std::vector<ValidationMessage> messages;

    bool ok() const;
    void evidence_gap(const std::string& text);
    void error(const std::string& text);
    void warning(const std::string& text);
    std::string format() const;
};

ValidationReport validate_config(const ModelConfig& config);

} // namespace flash_model

#endif
