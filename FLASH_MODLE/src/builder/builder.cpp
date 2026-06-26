#include "flash_model/builder.hpp"

#include "flash_model/loader.hpp"
#include "flash_model/validator.hpp"

#include <stdexcept>

namespace flash_model {

ValidationReport validate_model_config(ModelConfig config)
{
    std::unique_ptr<DevicePolicy> policy = create_device_policy(config);
    policy->apply_defaults(config);

    ValidationReport report = validate_config(config);
    std::vector<std::unique_ptr<CapabilityModule>> modules = create_capability_modules(config);
    for (const auto& module : modules) module->validate(config, report);
    policy->validate(config, report);
    return report;
}

FlashModel build_model(ModelConfig config)
{
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

FlashModel build_model_from_file(const std::string& path)
{
    return build_model(load_config_file(path));
}

} // namespace flash_model
