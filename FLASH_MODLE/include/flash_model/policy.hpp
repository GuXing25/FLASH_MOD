#ifndef FLASH_MODEL_POLICY_HPP
#define FLASH_MODEL_POLICY_HPP

#include "flash_model/capability.hpp"
#include "flash_model/config.hpp"
#include "flash_model/validator.hpp"

#include <memory>
#include <string>

namespace flash_model {

class DevicePolicy {
public:
    virtual ~DevicePolicy() = default;

    // 策略名用于配置选择和 CLI 展示。
    virtual std::string name() const = 0;
    // apply_defaults 在 validate/build 前补齐策略默认值。
    virtual void apply_defaults(ModelConfig& config) const = 0;
    // validate 只检查策略相关规则，不替代通用 validator。
    virtual void validate(const ModelConfig& config, ValidationReport& report) const = 0;
    // on_power_on 在模型构造或 reset 后更新运行态默认值。
    virtual void on_power_on(RuntimeState& state) const = 0;
};

// 根据 policy.name 创建策略；未知策略应回退或报告为通用策略。
std::unique_ptr<DevicePolicy> create_device_policy(const ModelConfig& config);

} // namespace flash_model

#endif
