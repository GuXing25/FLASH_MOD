#ifndef FLASH_MODEL_VALIDATOR_HPP
#define FLASH_MODEL_VALIDATOR_HPP

#include "flash_model/config.hpp"

#include <string>
#include <vector>

namespace flash_model {

enum class ValidationLevel {
    EvidenceGap, // 缺字段来源证据；不阻止构建，但提示需要回溯 datasheet。
    Warning,     // 可运行但值得人工确认的问题。
    Error        // 配置非法，builder 应拒绝构建。
};

struct ValidationMessage {
    ValidationLevel level = ValidationLevel::Warning; // 消息级别。
    std::string text; // 人类可读说明。
};

struct ValidationReport {
    std::vector<ValidationMessage> messages; // 按发现顺序保存所有校验消息。

    bool ok() const;
    void evidence_gap(const std::string& text);
    void error(const std::string& text);
    void warning(const std::string& text);
    std::string format() const;
};

// 通用配置验证入口；policy 和 capability 仍可追加自己的校验。
ValidationReport validate_config(const ModelConfig& config);

} // namespace flash_model

#endif
