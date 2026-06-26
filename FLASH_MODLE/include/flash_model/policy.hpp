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

    virtual std::string name() const = 0;
    virtual void apply_defaults(ModelConfig& config) const = 0;
    virtual void validate(const ModelConfig& config, ValidationReport& report) const = 0;
    virtual void on_power_on(RuntimeState& state) const = 0;
};

std::unique_ptr<DevicePolicy> create_device_policy(const ModelConfig& config);

} // namespace flash_model

#endif
