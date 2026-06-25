#include "flash_model/validator.hpp"

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

void ValidationReport::warning(const std::string& text)
{
    messages.push_back({ValidationLevel::Warning, text});
}

std::string ValidationReport::format() const
{
    std::ostringstream out;
    for (const auto& message : messages) {
        out << (message.level == ValidationLevel::Error ? "error: " : "warning: ")
            << message.text << "\n";
    }
    return out.str();
}

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
    if (config.policy.name.empty()) {
        report.error("policy.name is required");
    }
    if (config.source.datasheet.empty()) {
        report.warning("source.datasheet is empty; later debugging will be harder");
    }

    const bool nand = is_nand_like(config.device.cls);
    if (!nand) {
        if (config.geometry.memory_size == 0) report.error("NOR requires geometry.memory_size");
        if (config.geometry.page_size == 0) report.error("NOR requires geometry.page_size");
        if (config.geometry.sector_size == 0) report.error("NOR requires geometry.sector_size");
        if (config.commands.page_read || config.commands.program_load ||
            config.commands.program_execute || config.commands.block_erase) {
            report.error("NOR config enables SPI-NAND-only commands");
        }
        if (config.capabilities.bad_block_management || config.capabilities.copy_back ||
            config.capabilities.ecc_status || config.capabilities.die_select) {
            report.error("NOR config enables NAND-only capabilities");
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
        } else if (config.capabilities.block_protect) {
            report.warning("block_protect is enabled for NOR but nor_protect_length is zero");
        }
    } else {
        if (config.geometry.main_size == 0) report.error("NAND requires geometry.main_size");
        if (config.geometry.pages_per_block == 0) report.error("NAND requires geometry.pages_per_block");
        if (config.geometry.blocks == 0) report.error("NAND requires geometry.blocks");
        if (config.effective_page_size() == 0) report.error("NAND effective page size is zero");
        if (config.commands.nor_read || config.commands.nor_program || config.commands.nor_erase) {
            report.error("NAND config enables NOR-only commands");
        }
        if (config.capabilities.four_byte_address) {
            report.error("four_byte_address is currently a NOR capability");
        }
        if (config.constraints.nor_protect_length != 0) {
            report.error("NAND config sets NOR-only protect range");
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
