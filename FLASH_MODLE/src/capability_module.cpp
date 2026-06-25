#include "flash_model/capability_module.hpp"

#include <utility>

namespace flash_model {
namespace {

enum class CapabilityKind {
    QuadEnable,
    FourByteAddress,
    BlockProtect,
    SuspendResume,
    Otp,
    SecurityRegister,
    EccStatus,
    BadBlockManagement,
    CopyBack,
    DieSelect,
    PlaneSelect,
    ReadRetry
};

std::string capability_name(CapabilityKind kind)
{
    switch (kind) {
    case CapabilityKind::QuadEnable:
        return "quad_enable";
    case CapabilityKind::FourByteAddress:
        return "four_byte_address";
    case CapabilityKind::BlockProtect:
        return "block_protect";
    case CapabilityKind::SuspendResume:
        return "suspend_resume";
    case CapabilityKind::Otp:
        return "otp";
    case CapabilityKind::SecurityRegister:
        return "security_register";
    case CapabilityKind::EccStatus:
        return "ecc_status";
    case CapabilityKind::BadBlockManagement:
        return "bad_block_management";
    case CapabilityKind::CopyBack:
        return "copy_back";
    case CapabilityKind::DieSelect:
        return "die_select";
    case CapabilityKind::PlaneSelect:
        return "plane_select";
    case CapabilityKind::ReadRetry:
        return "read_retry";
    }
    return "unknown";
}

class SimpleCapabilityModule final : public CapabilityModule {
public:
    explicit SimpleCapabilityModule(CapabilityKind kind) : kind_(kind) {}

    std::string name() const override { return capability_name(kind_); }

    void validate(const ModelConfig& config, ValidationReport& report) const override
    {
        const bool nand = is_nand_like(config.device.cls);
        switch (kind_) {
        case CapabilityKind::FourByteAddress:
            if (nand) report.error("four_byte_address is not valid for NAND-like devices");
            break;
        case CapabilityKind::EccStatus:
        case CapabilityKind::BadBlockManagement:
        case CapabilityKind::CopyBack:
        case CapabilityKind::DieSelect:
        case CapabilityKind::PlaneSelect:
        case CapabilityKind::ReadRetry:
            if (!nand) report.error(name() + " is currently NAND-only");
            break;
        case CapabilityKind::SecurityRegister:
            if (nand) report.warning("security_register is usually NOR-specific; verify datasheet evidence");
            break;
        case CapabilityKind::QuadEnable:
        case CapabilityKind::BlockProtect:
        case CapabilityKind::SuspendResume:
        case CapabilityKind::Otp:
            break;
        }

        if (kind_ == CapabilityKind::EccStatus &&
            config.constraints.ecc_reserved_end == 0) {
            report.warning("ecc_status is enabled but ecc_reserved range is empty");
        }
    }

    void attach(RuntimeState& state) const override
    {
        state.enabled_capabilities.push_back(name());
        if (kind_ == CapabilityKind::QuadEnable) state.quad_enabled = true;
    }

private:
    CapabilityKind kind_;
};

void maybe_add(std::vector<std::unique_ptr<CapabilityModule>>& modules,
               bool enabled,
               CapabilityKind kind)
{
    if (enabled) modules.push_back(std::make_unique<SimpleCapabilityModule>(kind));
}

} // namespace

std::vector<std::unique_ptr<CapabilityModule>> create_capability_modules(const ModelConfig& config)
{
    std::vector<std::unique_ptr<CapabilityModule>> modules;
    maybe_add(modules, config.capabilities.quad_enable, CapabilityKind::QuadEnable);
    maybe_add(modules, config.capabilities.four_byte_address, CapabilityKind::FourByteAddress);
    maybe_add(modules, config.capabilities.block_protect, CapabilityKind::BlockProtect);
    maybe_add(modules, config.capabilities.suspend_resume, CapabilityKind::SuspendResume);
    maybe_add(modules, config.capabilities.otp, CapabilityKind::Otp);
    maybe_add(modules, config.capabilities.security_register, CapabilityKind::SecurityRegister);
    maybe_add(modules, config.capabilities.ecc_status, CapabilityKind::EccStatus);
    maybe_add(modules, config.capabilities.bad_block_management, CapabilityKind::BadBlockManagement);
    maybe_add(modules, config.capabilities.copy_back, CapabilityKind::CopyBack);
    maybe_add(modules, config.capabilities.die_select, CapabilityKind::DieSelect);
    maybe_add(modules, config.capabilities.plane_select, CapabilityKind::PlaneSelect);
    maybe_add(modules, config.capabilities.read_retry, CapabilityKind::ReadRetry);
    return modules;
}

} // namespace flash_model
