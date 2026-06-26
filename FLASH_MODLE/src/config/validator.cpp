#include "flash_model/validator.hpp"

#include "flash_model/command.hpp"

#include <sstream>

namespace flash_model {

bool ValidationReport::ok() const
{
    for (const auto& message : messages) {
        if (message.level == ValidationLevel::Error) return false;
    }
    return true;
}

void ValidationReport::error(const std::string& text)
{
    messages.push_back({ValidationLevel::Error, text});
}

void ValidationReport::evidence_gap(const std::string& text)
{
    messages.push_back({ValidationLevel::EvidenceGap, text});
}

void ValidationReport::warning(const std::string& text)
{
    messages.push_back({ValidationLevel::Warning, text});
}

std::string ValidationReport::format() const
{
    std::ostringstream out;
    for (const auto& message : messages) {
        if (message.level == ValidationLevel::Error) out << "error: ";
        else if (message.level == ValidationLevel::EvidenceGap) out << "evidence: ";
        else out << "warning: ";
        out << message.text << "\n";
    }
    return out.str();
}

namespace {

void require_evidence(const ModelConfig& config, ValidationReport& report, const std::string& field)
{
    if (!config.has_evidence(field)) {
        report.evidence_gap("missing field evidence for " + field);
    }
}

void require_common_evidence(const ModelConfig& config, ValidationReport& report)
{
    require_evidence(config, report, "device.name");
    require_evidence(config, report, "device.class");
    if (config.commands.read_id) require_evidence(config, report, "device.id");
    require_evidence(config, report, "policy.name");
}

} // namespace

ValidationReport validate_config(const ModelConfig& config)
{
    ValidationReport report;

    if (config.schema_version != 1) {
        report.error("unsupported schema_version: " + std::to_string(config.schema_version));
    }
    if (config.device.name.empty()) report.error("device.name is required");
    if (config.commands.read_id && config.device.id.empty()) {
        report.warning("commands.read_id is true but device.id is empty");
    }
    if (config.geometry.planes == 0) report.error("geometry.planes must be greater than zero");
    if (config.geometry.dies == 0) report.error("geometry.dies must be greater than zero");
    if (config.constraints.address_bytes != 3 && config.constraints.address_bytes != 4) {
        report.error("constraints.address_bytes must be 3 or 4");
    }
    if (config.policy.name.empty()) {
        report.error("policy.name is required");
    }
    if (config.source.datasheet.empty()) {
        report.warning("source.datasheet is empty; later debugging will be harder");
    }
    require_common_evidence(config, report);

    if (config.interface.enabled) {
        if (config.interface.protocol != "generic_chiplet") {
            report.error("interface.protocol must be generic_chiplet when interface.enabled is true");
        }
        if (config.interface.lanes == 0) {
            report.error("interface.lanes must be greater than zero");
        }
        if (config.interface.data_width_bits == 0) {
            report.error("interface.data_width_bits must be greater than zero");
        }
        if (config.interface.clock_mhz <= 0.0) {
            report.error("interface.clock_mhz must be greater than zero when enabled");
        }
        if (config.interface.fixed_latency_us < 0.0) {
            report.error("interface.fixed_latency_us must not be negative");
        }
        if (config.interface.turnaround_us < 0.0) {
            report.error("interface.turnaround_us must not be negative");
        }
    } else if (config.interface.protocol != "none" &&
               config.interface.protocol != "disabled") {
        report.warning("interface.protocol is set but interface.enabled is false");
    }

    for (const auto& item : config.transactions) {
        const std::string& name = item.first;
        const TransactionConfig& transaction = item.second;
        if (name.empty()) {
            report.error("transactions contains an empty command name");
            continue;
        }
        if (!is_known_transaction(name) && transaction.alias_of.empty()) {
            report.warning("transactions." + name + " overrides an unknown command name");
        }
        if (!transaction.alias_of.empty()) {
            if (transaction.alias_of == name) {
                report.error("transactions." + name + ".alias_of must not point to itself");
            } else if (!is_known_transaction(transaction.alias_of) &&
                       config.transactions.find(transaction.alias_of) == config.transactions.end()) {
                report.error("transactions." + name + ".alias_of points to unknown command: " +
                             transaction.alias_of);
            }
        }
        if (transaction.opcode_bytes == 0 &&
            transaction.fixed_request_bytes == 0 &&
            transaction.fixed_response_bytes == 0 &&
            transaction.alias_of.empty()) {
            report.error("transactions." + name + " has no opcode or fixed bytes");
        }
        if (transaction.has_opcode_value && transaction.opcode_bytes == 0) {
            report.error("transactions." + name + " sets opcode_value but opcode_bytes is zero");
        }
        if (transaction.has_opcode_value && transaction.opcode_bytes < 4) {
            const std::uint64_t max_opcode = 1ull << (transaction.opcode_bytes * 8u);
            if (transaction.opcode_value >= max_opcode) {
                report.error("transactions." + name + ".opcode_value does not fit opcode_bytes");
            }
        }
        if (transaction.address_bytes > 8) {
            report.error("transactions." + name + ".address_bytes is unexpectedly large");
        }
        if (transaction.command_lanes == 0 ||
            transaction.address_lanes == 0 ||
            transaction.data_lanes == 0) {
            report.error("transactions." + name + " lane counts must be greater than zero");
        }
        if (transaction.dummy_cycles != 0 && transaction.dummy_bytes != 0) {
            report.warning("transactions." + name +
                           " sets both dummy_cycles and dummy_bytes; byte timing still uses dummy_bytes");
        }
        if (transaction.use_current_address_bytes && transaction.address_bytes != 0) {
            report.warning("transactions." + name +
                           " sets address_bytes while use_current_address_bytes is true");
        }
        if (!transaction.turnaround &&
            (transaction.read_response || transaction.fixed_response_bytes != 0)) {
            report.warning("transactions." + name +
                           " has response bytes but turnaround is false");
        }
    }

    const bool nand = is_nand_like(config.device.cls);
    if (!nand) {
        if (config.geometry.memory_size == 0) report.error("NOR requires geometry.memory_size");
        if (config.geometry.page_size == 0) report.error("NOR requires geometry.page_size");
        if (config.geometry.sector_size == 0) report.error("NOR requires geometry.sector_size");
        if (config.commands.nor_block32_erase && config.geometry.block32_size == 0) {
            report.error("NOR 32KB block erase requires geometry.block32_size");
        }
        if (config.commands.nor_block_erase && config.geometry.block_size == 0) {
            report.error("NOR block erase requires geometry.block_size");
        }
        if (config.commands.nor_block32_erase && config.timing.block32_erase_us <= 0.0) {
            report.warning("NOR 32KB block erase is enabled but timing.block32_erase_us is zero");
        }
        if (config.commands.nor_block_erase && config.timing.block_erase_us <= 0.0) {
            report.warning("NOR block erase is enabled but timing.block_erase_us is zero");
        }
        if (config.commands.nor_chip_erase && config.timing.chip_erase_us <= 0.0) {
            report.warning("NOR chip erase is enabled but timing.chip_erase_us is zero");
        }
        if ((config.commands.deep_power_down || config.commands.release_power_down) &&
            !config.capabilities.deep_power_down) {
            report.warning("deep power-down commands are enabled but capabilities.deep_power_down is false");
        }
        if ((config.commands.enter_4byte_address || config.commands.exit_4byte_address) &&
            !config.capabilities.four_byte_address) {
            report.warning("4-byte address commands are enabled but capabilities.four_byte_address is false");
        }
        if (config.capabilities.four_byte_address &&
            config.geometry.memory_size <= 0x01000000ull) {
            report.warning("four_byte_address is enabled for a NOR device <= 16MiB; verify datasheet evidence");
        }
        if ((config.commands.suspend || config.commands.resume) &&
            !config.capabilities.suspend_resume) {
            report.warning("suspend/resume commands are enabled but capabilities.suspend_resume is false");
        }
        if (config.commands.read_unique_id && !config.capabilities.unique_id) {
            report.warning("read_unique_id command is enabled but capabilities.unique_id is false");
        }
        if (config.capabilities.unique_id) {
            if (!config.commands.read_unique_id) {
                report.warning("unique_id capability is enabled but read_unique_id command is false");
            }
            if (config.constraints.unique_id_size == 0) {
                report.error("unique_id requires constraints.unique_id_size > 0");
            }
            if (!config.device.unique_id.empty() &&
                config.device.unique_id.size() != config.constraints.unique_id_size) {
                report.warning("device.unique_id length differs from constraints.unique_id_size");
            }
            require_evidence(config, report, "constraints.unique_id_size");
        }
        if (config.commands.read_sfdp && !config.capabilities.sfdp) {
            report.warning("read_sfdp command is enabled but capabilities.sfdp is false");
        }
        if (config.capabilities.sfdp) {
            if (!config.commands.read_sfdp) {
                report.warning("sfdp capability is enabled but read_sfdp command is false");
            }
            if (config.constraints.sfdp_size == 0) {
                report.error("sfdp requires constraints.sfdp_size > 0");
            }
            require_evidence(config, report, "constraints.sfdp_size");
        }
        if ((config.commands.read_security_register || config.commands.program_security_register ||
             config.commands.erase_security_register) && !config.capabilities.security_register) {
            report.warning("security register commands are enabled but capabilities.security_register is false");
        }
        if (config.commands.lock_security_register && !config.capabilities.security_register) {
            report.warning("lock_security_register is enabled but capabilities.security_register is false");
        }
        if (config.capabilities.security_register) {
            if (config.constraints.security_register_count == 0) {
                report.error("security_register requires constraints.security_register_count > 0");
            }
            if (config.constraints.security_register_size == 0) {
                report.error("security_register requires constraints.security_register_size > 0");
            }
        }
        require_evidence(config, report, "geometry.memory_size");
        require_evidence(config, report, "geometry.page_size");
        require_evidence(config, report, "geometry.sector_size");
        if (config.commands.nor_block32_erase) require_evidence(config, report, "geometry.block32_size");
        if (config.commands.nor_block_erase) require_evidence(config, report, "geometry.block_size");
        if (config.commands.page_read || config.commands.program_load ||
            config.commands.program_execute || config.commands.block_erase ||
            config.commands.copy_back || config.commands.read_parameter_page) {
            report.error("NOR config enables SPI-NAND-only commands");
        }
        if (config.capabilities.bad_block_management || config.capabilities.copy_back ||
            config.capabilities.ecc_status || config.capabilities.die_select ||
            config.capabilities.plane_select || config.capabilities.read_retry ||
            config.capabilities.parameter_page) {
            report.error("NOR config enables NAND-only capabilities");
        }
        if (config.commands.enter_otp_mode || config.commands.exit_otp_mode) {
            report.warning("NOR OTP mode commands are modeled through security_register, not NAND-style OTP pages");
        }
        if (!config.constraints.initial_bad_blocks.empty()) {
            report.error("NOR config sets NAND-only initial_bad_blocks");
        }
        if (config.constraints.nor_protect_length != 0) {
            const std::uint64_t end = config.constraints.nor_protect_start +
                                      config.constraints.nor_protect_length;
            if (!config.capabilities.block_protect) {
                report.warning("NOR protect range is set but capabilities.block_protect is false");
            }
            if (end > config.geometry.memory_size || end < config.constraints.nor_protect_start) {
                report.error("NOR protect range exceeds memory_size");
            }
            require_evidence(config, report, "constraints.nor_protect_start");
            require_evidence(config, report, "constraints.nor_protect_length");
        } else if (config.capabilities.block_protect) {
            report.warning("block_protect is enabled for NOR but nor_protect_length is zero");
        }
    } else {
        if (config.geometry.main_size == 0) report.error("NAND requires geometry.main_size");
        if (config.geometry.pages_per_block == 0) report.error("NAND requires geometry.pages_per_block");
        if (config.geometry.blocks == 0) report.error("NAND requires geometry.blocks");
        if (config.effective_page_size() == 0) report.error("NAND effective page size is zero");
        require_evidence(config, report, "geometry.main_size");
        require_evidence(config, report, "geometry.spare_size");
        require_evidence(config, report, "geometry.pages_per_block");
        require_evidence(config, report, "geometry.blocks");
        if (config.commands.nor_read || config.commands.nor_program || config.commands.nor_erase ||
            config.commands.nor_block32_erase || config.commands.nor_block_erase ||
            config.commands.nor_chip_erase || config.commands.read_sfdp ||
            config.commands.deep_power_down || config.commands.release_power_down ||
            config.commands.read_security_register || config.commands.program_security_register ||
            config.commands.erase_security_register) {
            report.error("NAND config enables NOR-only commands");
        }
        if (config.capabilities.deep_power_down) {
            report.error("deep_power_down is currently a NOR capability");
        }
        if (config.capabilities.four_byte_address) {
            report.error("four_byte_address is currently a NOR capability");
        }
        if (config.capabilities.sfdp) {
            report.error("sfdp is currently a NOR capability");
        }
        if (config.commands.read_unique_id && !config.capabilities.unique_id) {
            report.warning("read_unique_id command is enabled but capabilities.unique_id is false");
        }
        if (config.capabilities.unique_id) {
            if (!config.commands.read_unique_id) {
                report.warning("unique_id capability is enabled but read_unique_id command is false");
            }
            if (config.constraints.unique_id_size == 0) {
                report.error("unique_id requires constraints.unique_id_size > 0");
            }
            if (!config.device.unique_id.empty() &&
                config.device.unique_id.size() != config.constraints.unique_id_size) {
                report.warning("device.unique_id length differs from constraints.unique_id_size");
            }
            require_evidence(config, report, "constraints.unique_id_size");
        }
        if ((config.commands.suspend || config.commands.resume) &&
            !config.capabilities.suspend_resume) {
            report.warning("suspend/resume commands are enabled but capabilities.suspend_resume is false");
        }
        if ((config.commands.enter_otp_mode || config.commands.exit_otp_mode) &&
            !config.capabilities.otp) {
            report.warning("OTP mode commands are enabled but capabilities.otp is false");
        }
        if (config.capabilities.otp && config.constraints.otp_page_count == 0) {
            report.warning("otp is enabled but constraints.otp_page_count is zero");
        }
        if (config.constraints.otp_page_count > 0) {
            if (!config.capabilities.otp) {
                report.warning("otp_page_count is set but capabilities.otp is false");
            }
            if (config.constraints.otp_page_count > config.geometry.blocks * config.geometry.pages_per_block) {
                report.error("otp_page_count exceeds NAND total page count");
            }
        }
        if (config.commands.copy_back && !config.capabilities.copy_back) {
            report.warning("commands.copy_back is true but capabilities.copy_back is false");
        }
        if (config.capabilities.copy_back && !config.commands.copy_back) {
            report.warning("capabilities.copy_back is true but commands.copy_back is false");
        }
        if (config.constraints.nor_protect_length != 0) {
            report.error("NAND config sets NOR-only protect range");
        }
        if (!config.constraints.initial_bad_blocks.empty()) {
            if (!config.capabilities.bad_block_management) {
                report.warning("initial_bad_blocks is set but capabilities.bad_block_management is false");
            }
            require_evidence(config, report, "constraints.initial_bad_blocks");
            for (std::uint32_t block : config.constraints.initial_bad_blocks) {
                if (block >= config.geometry.blocks) {
                    report.error("initial_bad_blocks contains block out of range: " +
                                 std::to_string(block));
                }
            }
        } else if (config.capabilities.bad_block_management) {
            report.warning("bad_block_management is enabled but initial_bad_blocks is empty");
        }
        if (config.constraints.bad_block_marker_offset != 0) {
            if (config.constraints.bad_block_marker_offset >= config.effective_page_size()) {
                report.error("bad_block_marker_offset must be inside the effective NAND page");
            }
            if (!config.capabilities.bad_block_management) {
                report.warning("bad_block_marker_offset is set but capabilities.bad_block_management is false");
            }
            require_evidence(config, report, "constraints.bad_block_marker_offset");
        }
        if (config.constraints.minimum_valid_blocks != 0) {
            if (config.constraints.minimum_valid_blocks > config.geometry.blocks) {
                report.error("minimum_valid_blocks exceeds geometry.blocks");
            }
            if (!config.capabilities.bad_block_management) {
                report.warning("minimum_valid_blocks is set but capabilities.bad_block_management is false");
            }
            require_evidence(config, report, "constraints.minimum_valid_blocks");
        }
        if (config.constraints.ecc_bits != 0) {
            if (!config.capabilities.ecc_status) {
                report.warning("ecc_bits is set but capabilities.ecc_status is false");
            }
            require_evidence(config, report, "constraints.ecc_bits");
        }
        if (config.capabilities.die_select && config.geometry.dies <= 1) {
            report.warning("die_select is enabled but geometry.dies is not greater than one");
        }
        if (config.capabilities.plane_select && config.geometry.planes <= 1) {
            report.warning("plane_select is enabled but geometry.planes is not greater than one");
        }
        if (config.capabilities.read_retry && config.constraints.read_retry_levels == 0) {
            report.warning("read_retry is enabled but constraints.read_retry_levels is zero");
        }
        if (!config.capabilities.read_retry && config.constraints.read_retry_levels != 0) {
            report.warning("read_retry_levels is set but capabilities.read_retry is false");
        }
        if (config.commands.read_parameter_page && !config.capabilities.parameter_page) {
            report.warning("read_parameter_page command is enabled but capabilities.parameter_page is false");
        }
        if (config.capabilities.parameter_page) {
            if (!config.commands.read_parameter_page) {
                report.warning("parameter_page capability is enabled but read_parameter_page command is false");
            }
            if (config.constraints.parameter_page_size == 0) {
                report.error("parameter_page requires constraints.parameter_page_size > 0");
            }
            require_evidence(config, report, "constraints.parameter_page_size");
        }
    }

    if (config.constraints.ecc_reserved_end != 0) {
        if (config.constraints.ecc_reserved_start >= config.constraints.ecc_reserved_end) {
            report.error("ecc_reserved_start must be smaller than ecc_reserved_end");
        }
        if (config.constraints.ecc_reserved_end > config.effective_page_size()) {
            report.error("ecc reserved range exceeds effective page size");
        }
        if (!config.capabilities.ecc_status) {
            report.warning("ecc reserved range is set but capabilities.ecc_status is false");
        }
        require_evidence(config, report, "constraints.ecc_reserved_start");
        require_evidence(config, report, "constraints.ecc_reserved_end");
    }

    if (config.constraints.max_partial_programs == 0) {
        report.error("max_partial_programs must be greater than zero");
    }

    if (config.unsupported_features.empty()) {
        report.warning("unsupported_features is empty; consider recording known unsupported datasheet features");
    }

    return report;
}

} // namespace flash_model
