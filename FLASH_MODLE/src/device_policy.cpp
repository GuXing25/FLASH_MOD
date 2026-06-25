#include "flash_model/device_policy.hpp"

#include <stdexcept>
#include <utility>

namespace flash_model {
namespace {

class GenericNorPolicy final : public DevicePolicy {
public:
    std::string name() const override { return "generic_nor"; }

    void apply_defaults(ModelConfig& config) const override
    {
        if (config.policy.name.empty()) config.policy.name = name();
        if (config.geometry.block_size == 0) config.geometry.block_size = config.geometry.sector_size;
    }

    void validate(const ModelConfig& config, ValidationReport& report) const override
    {
        if (is_nand_like(config.device.cls)) {
            report.error("generic_nor policy received a NAND-like config");
        }
    }

    void on_power_on(RuntimeState& state) const override
    {
        state.quad_enabled = state.quad_enabled || ((state.status2 & 0x02u) != 0);
    }
};

class GenericSpiNandPolicy final : public DevicePolicy {
public:
    std::string name() const override { return "generic_spinand"; }

    void apply_defaults(ModelConfig& config) const override
    {
        if (config.policy.name.empty()) config.policy.name = name();
        if (config.geometry.page_size == 0) {
            config.geometry.page_size = config.geometry.main_size + config.geometry.spare_size;
        }
        if (config.constraints.max_partial_programs == 0) {
            config.constraints.max_partial_programs = 4;
        }
    }

    void validate(const ModelConfig& config, ValidationReport& report) const override
    {
        if (!is_nand_like(config.device.cls)) {
            report.error("generic_spinand policy received a NOR config");
        }
    }

    void on_power_on(RuntimeState&) const override {}
};

class FamilyPolicy final : public DevicePolicy {
public:
    FamilyPolicy(std::string policy_name, bool nand_like)
        : policy_name_(std::move(policy_name)), nand_like_(nand_like)
    {
    }

    std::string name() const override { return policy_name_; }

    void apply_defaults(ModelConfig& config) const override
    {
        if (nand_like_) {
            GenericSpiNandPolicy{}.apply_defaults(config);
        } else {
            GenericNorPolicy{}.apply_defaults(config);
        }
    }

    void validate(const ModelConfig& config, ValidationReport& report) const override
    {
        if (nand_like_ != is_nand_like(config.device.cls)) {
            report.error(policy_name_ + " policy does not match device class");
        }
        report.warning(policy_name_ + " currently uses generic behavior plus future extension hooks");
    }

    void on_power_on(RuntimeState& state) const override
    {
        if (!nand_like_) GenericNorPolicy{}.on_power_on(state);
    }

private:
    std::string policy_name_;
    bool nand_like_ = false;
};

} // namespace

std::unique_ptr<DevicePolicy> create_device_policy(const ModelConfig& config)
{
    const std::string& name = config.policy.name;
    const bool nand = is_nand_like(config.device.cls);

    if (name.empty()) {
        if (nand) return std::make_unique<GenericSpiNandPolicy>();
        return std::make_unique<GenericNorPolicy>();
    }
    if (name == "generic_nor") return std::make_unique<GenericNorPolicy>();
    if (name == "generic_spinand") return std::make_unique<GenericSpiNandPolicy>();
    if (name == "winbond_family" || name == "micron_family" || name == "gigadevice_family") {
        return std::make_unique<FamilyPolicy>(name, nand);
    }

    throw std::runtime_error("unknown policy: " + name);
}

} // namespace flash_model
