#include "flash_model/capability.hpp"

#include "flash_model/address.hpp"

#include <algorithm>
#include <utility>

namespace flash_model {
namespace {

enum class CapabilityKind {
    QuadEnable,
    FourByteAddress,
    BlockProtect,
    DeepPowerDown,
    SuspendResume,
    UniqueId,
    Sfdp,
    Otp,
    SecurityRegister,
    EccStatus,
    BadBlockManagement,
    CopyBack,
    DieSelect,
    PlaneSelect,
    ReadRetry,
    ParameterPage
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
    case CapabilityKind::DeepPowerDown:
        return "deep_power_down";
    case CapabilityKind::SuspendResume:
        return "suspend_resume";
    case CapabilityKind::UniqueId:
        return "unique_id";
    case CapabilityKind::Sfdp:
        return "sfdp";
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
    case CapabilityKind::ParameterPage:
        return "parameter_page";
    }
    return "unknown";
}

void attach_name(RuntimeState& state, const std::string& name)
{
    state.enabled_capabilities.push_back(name);
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
            if (!config.commands.enter_4byte_address || !config.commands.exit_4byte_address) {
                report.warning("four_byte_address capability expects enter_4byte_address and exit_4byte_address commands");
            }
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
            if (config.constraints.security_register_count == 0 ||
                config.constraints.security_register_size == 0) {
                report.error("security_register requires non-zero count and size constraints");
            }
            break;
        case CapabilityKind::DeepPowerDown:
            if (nand) report.error("deep_power_down is currently NOR-only");
            if (!config.commands.deep_power_down || !config.commands.release_power_down) {
                report.warning("deep_power_down capability expects both deep_power_down and release_power_down commands");
            }
            break;
        case CapabilityKind::UniqueId:
            if (!config.commands.read_unique_id) {
                report.warning("unique_id capability expects read_unique_id command");
            }
            if (config.constraints.unique_id_size == 0) {
                report.error("unique_id requires constraints.unique_id_size > 0");
            }
            break;
        case CapabilityKind::Sfdp:
            if (nand) report.error("sfdp is currently NOR-only");
            if (!config.commands.read_sfdp) {
                report.warning("sfdp capability expects read_sfdp command");
            }
            if (config.constraints.sfdp_size == 0) {
                report.error("sfdp requires constraints.sfdp_size > 0");
            }
            break;
        case CapabilityKind::QuadEnable:
        case CapabilityKind::BlockProtect:
            break;
        case CapabilityKind::SuspendResume:
            if (!config.commands.suspend || !config.commands.resume) {
                report.warning("suspend_resume capability expects suspend and resume commands");
            }
            break;
        case CapabilityKind::Otp:
            if (nand) {
                if (!config.commands.enter_otp_mode || !config.commands.exit_otp_mode) {
                    report.warning("NAND OTP capability expects enter_otp_mode and exit_otp_mode commands");
                }
                if (config.constraints.otp_page_count == 0) {
                    report.warning("NAND OTP capability has zero otp_page_count");
                }
            }
            break;
        case CapabilityKind::ParameterPage:
            if (!nand) report.error("parameter_page is currently NAND-only");
            if (!config.commands.read_parameter_page) {
                report.warning("parameter_page capability expects read_parameter_page command");
            }
            if (config.constraints.parameter_page_size == 0) {
                report.error("parameter_page requires constraints.parameter_page_size > 0");
            }
            break;
        }

        if (kind_ == CapabilityKind::EccStatus &&
            config.constraints.ecc_reserved_end == 0) {
            report.warning("ecc_status is enabled but ecc_reserved range is empty");
        }

        if (kind_ == CapabilityKind::DieSelect && config.geometry.dies <= 1) {
            report.warning("die_select is enabled but geometry.dies <= 1");
        }
        if (kind_ == CapabilityKind::PlaneSelect && config.geometry.planes <= 1) {
            report.warning("plane_select is enabled but geometry.planes <= 1");
        }
        if (kind_ == CapabilityKind::ReadRetry && config.constraints.read_retry_levels == 0) {
            report.warning("read_retry is enabled but read_retry_levels is zero");
        }
    }

    void attach(RuntimeState& state) const override
    {
        attach_name(state, name());
    }

private:
    CapabilityKind kind_;
};

class BlockProtectModule final : public CapabilityModule {
public:
    std::string name() const override { return "block_protect"; }

    void validate(const ModelConfig&, ValidationReport&) const override {}

    void attach(RuntimeState& state) const override
    {
        attach_name(state, name());
    }

    CapabilityDecision before_nor_program(const ModelConfig& config,
                                          const RuntimeState&,
                                          std::uint64_t address,
                                          std::uint64_t length) const override
    {
        if (is_nand_like(config.device.cls)) return CapabilityDecision::allow();
        if (config.constraints.nor_protect_length == 0) return CapabilityDecision::allow();
        if (!AddressMapper::ranges_overlap(address, length,
                                           config.constraints.nor_protect_start,
                                           config.constraints.nor_protect_length)) {
            return CapabilityDecision::allow();
        }
        return CapabilityDecision::reject("NOR_PROGRAM rejected: protected range");
    }

    CapabilityDecision before_nor_erase(const ModelConfig& config,
                                        const RuntimeState&,
                                        std::uint64_t address,
                                        std::uint64_t length) const override
    {
        if (is_nand_like(config.device.cls)) return CapabilityDecision::allow();
        if (config.constraints.nor_protect_length == 0) return CapabilityDecision::allow();
        if (!AddressMapper::ranges_overlap(address, length,
                                           config.constraints.nor_protect_start,
                                           config.constraints.nor_protect_length)) {
            return CapabilityDecision::allow();
        }
        return CapabilityDecision::reject("NOR_ERASE rejected: protected range");
    }

    CapabilityDecision before_nand_block_erase(const ModelConfig& config,
                                               const RuntimeState& state,
                                               std::uint32_t) const override
    {
        if (!is_nand_like(config.device.cls)) return CapabilityDecision::allow();
        const auto it = state.features.find(0xA0);
        const std::uint8_t a0 = it == state.features.end() ? 0 : it->second;
        if ((a0 & 0x7Cu) == 0) return CapabilityDecision::allow();
        return CapabilityDecision::reject("BLOCK_ERASE rejected: block protected");
    }

    CapabilityDecision before_nand_program_execute(const ModelConfig& config,
                                                   const RuntimeState& state,
                                                   std::uint32_t,
                                                   std::uint32_t,
                                                   bool) const override
    {
        if (!is_nand_like(config.device.cls)) return CapabilityDecision::allow();
        const auto it = state.features.find(0xA0);
        const std::uint8_t a0 = it == state.features.end() ? 0 : it->second;
        if ((a0 & 0x7Cu) == 0) return CapabilityDecision::allow();
        return CapabilityDecision::reject("PROGRAM_EXECUTE rejected: block protected");
    }
};

class EccStatusModule final : public CapabilityModule {
public:
    std::string name() const override { return "ecc_status"; }

    void validate(const ModelConfig& config, ValidationReport& report) const override
    {
        if (!is_nand_like(config.device.cls)) {
            report.error("ecc_status is currently NAND-only");
        }
        if (config.constraints.ecc_reserved_end == 0) {
            report.warning("ecc_status is enabled but ecc_reserved range is empty");
        }
    }

    void attach(RuntimeState& state) const override
    {
        attach_name(state, name());
    }

    bool marks_nand_program_load_violation(const ModelConfig& config,
                                           const RuntimeState&,
                                           std::uint16_t column,
                                           std::uint64_t length) const override
    {
        if (!is_nand_like(config.device.cls)) return false;
        if (config.constraints.ecc_reserved_end == 0 || length == 0) return false;
        return AddressMapper::ranges_overlap(column, length,
                                             config.constraints.ecc_reserved_start,
                                             config.constraints.ecc_reserved_end -
                                                 config.constraints.ecc_reserved_start);
    }

    CapabilityDecision before_nand_program_execute(const ModelConfig&,
                                                   const RuntimeState&,
                                                   std::uint32_t,
                                                   std::uint32_t,
                                                   bool cached_program_violation) const override
    {
        if (!cached_program_violation) return CapabilityDecision::allow();
        return CapabilityDecision::reject("PROGRAM_EXECUTE rejected: ECC reserved spare range");
    }
};

class BadBlockModule final : public CapabilityModule {
public:
    explicit BadBlockModule(const ModelConfig& config)
        : initial_bad_blocks_(config.constraints.initial_bad_blocks)
    {
    }

    std::string name() const override { return "bad_block_management"; }

    void validate(const ModelConfig& config, ValidationReport& report) const override
    {
        if (!is_nand_like(config.device.cls)) {
            report.error("bad_block_management is currently NAND-only");
            return;
        }

        if (initial_bad_blocks_.empty()) {
            report.warning("bad_block_management is enabled but initial_bad_blocks is empty");
            return;
        }

        for (std::uint32_t block : initial_bad_blocks_) {
            if (block >= config.geometry.blocks) {
                report.error("initial_bad_blocks contains block out of range: " +
                             std::to_string(block));
            }
        }
    }

    void attach(RuntimeState& state) const override
    {
        attach_name(state, name());
        for (std::uint32_t block : initial_bad_blocks_) {
            if (!is_bad_block(state, block)) state.bad_blocks.push_back(block);
        }
    }

    CapabilityDecision before_nand_block_erase(const ModelConfig& config,
                                               const RuntimeState& state,
                                               std::uint32_t block) const override
    {
        if (!is_nand_like(config.device.cls)) return CapabilityDecision::allow();
        if (!is_bad_block(state, block)) return CapabilityDecision::allow();
        return CapabilityDecision::reject("BLOCK_ERASE rejected: bad block");
    }

    CapabilityDecision before_nand_program_execute(const ModelConfig& config,
                                                   const RuntimeState& state,
                                                   std::uint32_t,
                                                   std::uint32_t block,
                                                   bool) const override
    {
        if (!is_nand_like(config.device.cls)) return CapabilityDecision::allow();
        if (!is_bad_block(state, block)) return CapabilityDecision::allow();
        return CapabilityDecision::reject("PROGRAM_EXECUTE rejected: bad block");
    }

private:
    static bool is_bad_block(const RuntimeState& state, std::uint32_t block)
    {
        return std::find(state.bad_blocks.begin(), state.bad_blocks.end(), block) !=
               state.bad_blocks.end();
    }

    std::vector<std::uint32_t> initial_bad_blocks_;
};

class CopyBackModule final : public CapabilityModule {
public:
    std::string name() const override { return "copy_back"; }

    void validate(const ModelConfig& config, ValidationReport& report) const override
    {
        if (!is_nand_like(config.device.cls)) {
            report.error("copy_back is currently NAND-only");
            return;
        }
        if (!config.commands.copy_back) {
            report.warning("copy_back capability is enabled but commands.copy_back is false");
        }
    }

    void attach(RuntimeState& state) const override
    {
        attach_name(state, name());
    }

    CapabilityDecision before_nand_copy_back(const ModelConfig& config,
                                             const RuntimeState&,
                                             std::uint32_t source_page,
                                             std::uint32_t target_page,
                                             std::uint32_t) const override
    {
        if (!is_nand_like(config.device.cls)) return CapabilityDecision::allow();
        if (source_page != target_page) return CapabilityDecision::allow();
        return CapabilityDecision::reject("COPY_BACK rejected: source and target page are the same");
    }
};

void maybe_add(std::vector<std::unique_ptr<CapabilityModule>>& modules,
               const ModelConfig& config,
               bool enabled,
               CapabilityKind kind)
{
    if (!enabled) return;
    if (kind == CapabilityKind::BlockProtect) {
        modules.push_back(std::make_unique<BlockProtectModule>());
        return;
    }
    if (kind == CapabilityKind::EccStatus) {
        modules.push_back(std::make_unique<EccStatusModule>());
        return;
    }
    if (kind == CapabilityKind::BadBlockManagement) {
        modules.push_back(std::make_unique<BadBlockModule>(config));
        return;
    }
    if (kind == CapabilityKind::CopyBack) {
        modules.push_back(std::make_unique<CopyBackModule>());
        return;
    }
    modules.push_back(std::make_unique<SimpleCapabilityModule>(kind));
}

} // namespace

CapabilityDecision CapabilityModule::before_nor_program(const ModelConfig&,
                                                        const RuntimeState&,
                                                        std::uint64_t,
                                                        std::uint64_t) const
{
    return CapabilityDecision::allow();
}

CapabilityDecision CapabilityModule::before_nor_erase(const ModelConfig&,
                                                      const RuntimeState&,
                                                      std::uint64_t,
                                                      std::uint64_t) const
{
    return CapabilityDecision::allow();
}

CapabilityDecision CapabilityModule::before_nand_block_erase(const ModelConfig&,
                                                             const RuntimeState&,
                                                             std::uint32_t) const
{
    return CapabilityDecision::allow();
}

CapabilityDecision CapabilityModule::before_nand_program_execute(const ModelConfig&,
                                                                 const RuntimeState&,
                                                                 std::uint32_t,
                                                                 std::uint32_t,
                                                                 bool) const
{
    return CapabilityDecision::allow();
}

CapabilityDecision CapabilityModule::before_nand_copy_back(const ModelConfig&,
                                                           const RuntimeState&,
                                                           std::uint32_t,
                                                           std::uint32_t,
                                                           std::uint32_t) const
{
    return CapabilityDecision::allow();
}

bool CapabilityModule::marks_nand_program_load_violation(const ModelConfig&,
                                                         const RuntimeState&,
                                                         std::uint16_t,
                                                         std::uint64_t) const
{
    return false;
}

std::vector<std::unique_ptr<CapabilityModule>> create_capability_modules(const ModelConfig& config)
{
    std::vector<std::unique_ptr<CapabilityModule>> modules;
    maybe_add(modules, config, config.capabilities.quad_enable, CapabilityKind::QuadEnable);
    maybe_add(modules, config, config.capabilities.four_byte_address, CapabilityKind::FourByteAddress);
    maybe_add(modules, config, config.capabilities.block_protect, CapabilityKind::BlockProtect);
    maybe_add(modules, config, config.capabilities.deep_power_down, CapabilityKind::DeepPowerDown);
    maybe_add(modules, config, config.capabilities.suspend_resume, CapabilityKind::SuspendResume);
    maybe_add(modules, config, config.capabilities.unique_id, CapabilityKind::UniqueId);
    maybe_add(modules, config, config.capabilities.sfdp, CapabilityKind::Sfdp);
    maybe_add(modules, config, config.capabilities.otp, CapabilityKind::Otp);
    maybe_add(modules, config, config.capabilities.security_register, CapabilityKind::SecurityRegister);
    maybe_add(modules, config, config.capabilities.ecc_status, CapabilityKind::EccStatus);
    maybe_add(modules, config, config.capabilities.bad_block_management, CapabilityKind::BadBlockManagement);
    maybe_add(modules, config, config.capabilities.copy_back, CapabilityKind::CopyBack);
    maybe_add(modules, config, config.capabilities.die_select, CapabilityKind::DieSelect);
    maybe_add(modules, config, config.capabilities.plane_select, CapabilityKind::PlaneSelect);
    maybe_add(modules, config, config.capabilities.read_retry, CapabilityKind::ReadRetry);
    maybe_add(modules, config, config.capabilities.parameter_page, CapabilityKind::ParameterPage);
    return modules;
}

} // namespace flash_model
