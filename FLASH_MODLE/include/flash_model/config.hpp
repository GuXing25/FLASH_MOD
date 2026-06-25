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
};

struct GeometryConfig {
    std::uint64_t memory_size = 0;
    std::uint32_t page_size = 0;
    std::uint32_t sector_size = 0;
    std::uint32_t block_size = 0;
    std::uint32_t main_size = 0;
    std::uint32_t spare_size = 0;
    std::uint32_t pages_per_block = 0;
    std::uint32_t blocks = 0;
    std::uint32_t planes = 1;
};

struct TimingConfig {
    double read_us = 0.0;
    double page_read_us = 0.0;
    double program_us = 0.0;
    double sector_erase_us = 0.0;
    double block_erase_us = 0.0;
    double chip_erase_us = 0.0;
    double reset_us = 0.0;
    double puw_us = 0.0;
};

struct CommandConfig {
    bool read_id = false;
    bool reset = false;
    bool nor_read = false;
    bool nor_program = false;
    bool nor_erase = false;
    bool page_read = false;
    bool read_from_cache = false;
    bool program_load = false;
    bool program_execute = false;
    bool block_erase = false;
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
    bool suspend_resume = false;
    bool otp = false;
    bool security_register = false;
    bool ecc_status = false;
    bool bad_block_management = false;
    bool copy_back = false;
    bool die_select = false;
    bool plane_select = false;
    bool read_retry = false;
};

struct ConstraintConfig {
    bool require_wel = true;
    bool require_qe_for_quad = true;
    bool default_all_protected = false;
    bool strict_sequential_program = false;
    std::uint32_t max_partial_programs = 4;
    std::uint32_t ecc_reserved_start = 0;
    std::uint32_t ecc_reserved_end = 0;
    std::uint64_t nor_protect_start = 0;
    std::uint64_t nor_protect_length = 0;
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
    CommandConfig commands;
    RegisterConfig registers;
    CapabilityConfig capabilities;
    ConstraintConfig constraints;
    PolicyConfig policy;
    Evidence source;
    std::vector<std::string> unsupported_features;

    std::uint32_t effective_page_size() const;
    std::uint64_t total_size_bytes() const;
};

} // namespace flash_model

#endif
