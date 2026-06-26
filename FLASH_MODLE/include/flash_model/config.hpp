#ifndef FLASH_MODEL_CONFIG_HPP
#define FLASH_MODEL_CONFIG_HPP

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace flash_model {

enum class FlashClass {
    Nor,
    SpiNand,
    RawNand
};

std::string to_string(FlashClass cls);
FlashClass flash_class_from_string(const std::string& text);
bool is_nand_like(FlashClass cls);

struct Evidence {
    std::string datasheet;
    std::string page;
    double confidence = 0.0;
};

struct DeviceConfig {
    std::string name;
    FlashClass cls = FlashClass::Nor;
    std::string manufacturer;
    std::string part_number;
    std::vector<std::uint8_t> id;
    std::vector<std::uint8_t> unique_id;
};

struct GeometryConfig {
    std::uint64_t memory_size = 0;
    std::uint32_t page_size = 0;
    std::uint32_t sector_size = 0;
    std::uint32_t block32_size = 0;
    std::uint32_t block_size = 0;
    std::uint32_t main_size = 0;
    std::uint32_t spare_size = 0;
    std::uint32_t pages_per_block = 0;
    std::uint32_t blocks = 0;
    std::uint32_t planes = 1;
    std::uint32_t dies = 1;
};

struct TimingConfig {
    double read_us = 0.0;
    double page_read_us = 0.0;
    double program_us = 0.0;
    double sector_erase_us = 0.0;
    double block32_erase_us = 0.0;
    double block_erase_us = 0.0;
    double chip_erase_us = 0.0;
    double reset_us = 0.0;
    double puw_us = 0.0;
};

// Behavior-level chiplet-to-chiplet link model. It intentionally describes
// transaction cost only: command bytes, address bytes, payload bytes, response
// bytes, link bandwidth, fixed latency, and packet overhead. It does not model
// pins, voltage, signal integrity, or cycle-accurate PHY behavior.
struct InterfaceConfig {
    bool enabled = false;
    std::string name = "disabled";
    std::string protocol = "none";
    std::uint32_t lanes = 1;
    std::uint32_t data_width_bits = 32;
    double clock_mhz = 0.0;
    double fixed_latency_us = 0.0;
    double turnaround_us = 0.0;
    std::uint32_t packet_overhead_bytes = 0;
    std::uint32_t max_payload_bytes = 0;
};

// Optional per-command transaction override. The default command table covers
// the common NOR/SPI-NAND facade. A profile only needs this section when its
// chiplet-facing transaction shape differs from the generic command shape.
struct TransactionConfig {
    std::uint32_t opcode_value = 0;
    bool has_opcode_value = false;
    std::uint32_t opcode_bytes = 1;
    std::uint32_t address_bytes = 0;
    std::uint32_t dummy_bytes = 0;
    std::uint32_t dummy_cycles = 0;
    std::uint32_t fixed_request_bytes = 0;
    std::uint32_t fixed_response_bytes = 0;
    std::uint32_t command_lanes = 1;
    std::uint32_t address_lanes = 1;
    std::uint32_t data_lanes = 1;
    bool use_current_address_bytes = false;
    bool write_payload = false;
    bool read_response = false;
    bool turnaround = true;
    std::string alias_of;
};

struct CommandConfig {
    bool read_id = false;
    bool reset = false;
    bool nor_read = false;
    bool nor_program = false;
    bool nor_erase = false;
    bool nor_block32_erase = false;
    bool nor_block_erase = false;
    bool nor_chip_erase = false;
    bool read_unique_id = false;
    bool read_sfdp = false;
    bool deep_power_down = false;
    bool release_power_down = false;
    bool enter_4byte_address = false;
    bool exit_4byte_address = false;
    bool suspend = false;
    bool resume = false;
    bool read_security_register = false;
    bool program_security_register = false;
    bool erase_security_register = false;
    bool lock_security_register = false;
    bool enter_otp_mode = false;
    bool exit_otp_mode = false;
    bool page_read = false;
    bool read_from_cache = false;
    bool read_parameter_page = false;
    bool program_load = false;
    bool program_execute = false;
    bool block_erase = false;
    bool copy_back = false;
};

struct RegisterConfig {
    std::uint8_t status1_default = 0;
    std::uint8_t status2_default = 0;
    std::uint8_t status3_default = 0;
    std::uint8_t feature_a0_default = 0;
    std::uint8_t feature_b0_default = 0;
    std::uint8_t feature_c0_default = 0;
    std::uint8_t feature_d0_default = 0;
    bool feature_c0_dynamic = false;
};

struct CapabilityConfig {
    bool quad_enable = false;
    bool four_byte_address = false;
    bool block_protect = false;
    bool deep_power_down = false;
    bool suspend_resume = false;
    bool unique_id = false;
    bool sfdp = false;
    bool otp = false;
    bool security_register = false;
    bool ecc_status = false;
    bool bad_block_management = false;
    bool copy_back = false;
    bool die_select = false;
    bool plane_select = false;
    bool read_retry = false;
    bool parameter_page = false;
};

struct ConstraintConfig {
    bool require_wel = true;
    bool require_qe_for_quad = true;
    bool default_all_protected = false;
    bool strict_sequential_program = false;
    std::uint32_t max_partial_programs = 4;
    std::uint32_t ecc_reserved_start = 0;
    std::uint32_t ecc_reserved_end = 0;
    std::vector<std::uint32_t> initial_bad_blocks;
    std::uint64_t nor_protect_start = 0;
    std::uint64_t nor_protect_length = 0;
    // API calls use 64-bit addresses, but this records the command/address
    // mode a real SPI NOR part would power up with.
    std::uint32_t address_bytes = 3;
    // NOR security registers and NAND OTP pages vary by vendor. Keeping the
    // sizes in config avoids hard-coding Winbond-like defaults in the model.
    std::uint32_t security_register_count = 3;
    std::uint32_t security_register_size = 256;
    std::uint32_t otp_page_count = 0;
    std::uint32_t read_retry_levels = 0;
    std::uint32_t unique_id_size = 8;
    std::uint32_t sfdp_size = 256;
    std::uint32_t parameter_page_size = 256;
    std::uint32_t bad_block_marker_offset = 0;
    std::uint32_t minimum_valid_blocks = 0;
    std::uint32_t ecc_bits = 0;
};

struct PolicyConfig {
    std::string name;
    std::map<std::string, std::string> overrides;
};

struct ModelConfig {
    int schema_version = 1;
    DeviceConfig device;
    GeometryConfig geometry;
    TimingConfig timing;
    InterfaceConfig interface;
    CommandConfig commands;
    RegisterConfig registers;
    CapabilityConfig capabilities;
    ConstraintConfig constraints;
    std::map<std::string, TransactionConfig> transactions;
    PolicyConfig policy;
    Evidence source;
    std::map<std::string, Evidence> field_evidence;
    std::vector<std::string> unsupported_features;

    std::uint32_t effective_page_size() const;
    std::uint64_t total_size_bytes() const;
    bool has_evidence(const std::string& field) const;
};

} // namespace flash_model

#endif
