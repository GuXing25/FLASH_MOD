#include "flash_model/model_builder.hpp"

#include "flash_model/config_loader.hpp"
#include "flash_model/validator.hpp"

#include <stdexcept>

namespace flash_model {

FlashModel build_model_from_file(const std::string& path)
{
    ModelConfig config = load_config_file(path);
    std::unique_ptr<DevicePolicy> policy = create_device_policy(config);
    policy->apply_defaults(config);

    ValidationReport report = validate_config(config);
    std::vector<std::unique_ptr<CapabilityModule>> modules = create_capability_modules(config);
    for (const auto& module : modules) module->validate(config, report);
    policy->validate(config, report);

    if (!report.ok()) {
        throw std::runtime_error("configuration validation failed:\n" + report.format());
    }

    return FlashModel(std::move(config), std::move(policy), std::move(modules));
}

} // namespace flash_model
