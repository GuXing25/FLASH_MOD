#include "flash_model/config_loader.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace flash_model {
namespace {

std::string trim(const std::string& text)
{
    std::size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first]))) ++first;
    std::size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1]))) --last;
    return text.substr(first, last - first);
}

std::string strip_comment(const std::string& line)
{
    bool in_single = false;
    bool in_double = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '\'' && !in_double) in_single = !in_single;
        if (c == '"' && !in_single) in_double = !in_double;
        if (c == '#' && !in_single && !in_double) return line.substr(0, i);
    }
    return line;
}

std::string unquote(std::string value)
{
    value = trim(value);
    if (value.size() >= 2) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return value.substr(1, value.size() - 2);
        }
    }
    return value;
}

std::uint64_t parse_u64(const std::string& value)
{
    std::size_t used = 0;
    const std::uint64_t parsed = std::stoull(unquote(value), &used, 0);
    if (used == 0) throw std::runtime_error("invalid integer: " + value);
    return parsed;
}

std::uint32_t parse_u32(const std::string& value)
{
    return static_cast<std::uint32_t>(parse_u64(value));
}

std::uint8_t parse_u8(const std::string& value)
{
    return static_cast<std::uint8_t>(parse_u64(value) & 0xFFu);
}

double parse_double(const std::string& value)
{
    return std::stod(unquote(value));
}

bool parse_bool(const std::string& value)
{
    std::string v = unquote(value);
    std::transform(v.begin(), v.end(), v.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (v == "true" || v == "yes" || v == "1") return true;
    if (v == "false" || v == "no" || v == "0") return false;
    throw std::runtime_error("invalid boolean: " + value);
}

std::vector<std::uint8_t> parse_id_list(std::string value)
{
    value = unquote(value);
    for (char& c : value) {
        if (c == '[' || c == ']' || c == ',') c = ' ';
    }

    std::vector<std::uint8_t> id;
    std::istringstream input(value);
    std::string token;
    while (input >> token) {
        id.push_back(parse_u8(token));
    }
    return id;
}

std::size_t leading_spaces(const std::string& line)
{
    std::size_t n = 0;
    while (n < line.size() && line[n] == ' ') ++n;
    return n;
}

void assign_value(ModelConfig& config,
                  const std::string& section,
                  const std::string& key,
                  const std::string& value)
{
    if (section == "device") {
        if (key == "name") config.device.name = unquote(value);
        else if (key == "class") config.device.cls = flash_class_from_string(unquote(value));
        else if (key == "manufacturer") config.device.manufacturer = unquote(value);
        else if (key == "part_number") config.device.part_number = unquote(value);
        else if (key == "id") config.device.id = parse_id_list(value);
        return;
    }

    if (section == "geometry") {
        if (key == "memory_size") config.geometry.memory_size = parse_u64(value);
        else if (key == "page_size") config.geometry.page_size = parse_u32(value);
        else if (key == "sector_size") config.geometry.sector_size = parse_u32(value);
        else if (key == "block_size") config.geometry.block_size = parse_u32(value);
        else if (key == "main_size") config.geometry.main_size = parse_u32(value);
        else if (key == "spare_size") config.geometry.spare_size = parse_u32(value);
        else if (key == "pages_per_block") config.geometry.pages_per_block = parse_u32(value);
        else if (key == "blocks") config.geometry.blocks = parse_u32(value);
        else if (key == "planes") config.geometry.planes = parse_u32(value);
        return;
    }

    if (section == "timing") {
        if (key == "read_us") config.timing.read_us = parse_double(value);
        else if (key == "page_read_us") config.timing.page_read_us = parse_double(value);
        else if (key == "program_us") config.timing.program_us = parse_double(value);
        else if (key == "sector_erase_us") config.timing.sector_erase_us = parse_double(value);
        else if (key == "block_erase_us") config.timing.block_erase_us = parse_double(value);
        else if (key == "chip_erase_us") config.timing.chip_erase_us = parse_double(value);
        else if (key == "reset_us") config.timing.reset_us = parse_double(value);
        else if (key == "puw_us") config.timing.puw_us = parse_double(value);
        return;
    }

    if (section == "commands") {
        if (key == "read_id") config.commands.read_id = parse_bool(value);
        else if (key == "reset") config.commands.reset = parse_bool(value);
        else if (key == "nor_read") config.commands.nor_read = parse_bool(value);
        else if (key == "nor_program") config.commands.nor_program = parse_bool(value);
        else if (key == "nor_erase") config.commands.nor_erase = parse_bool(value);
        else if (key == "page_read") config.commands.page_read = parse_bool(value);
        else if (key == "read_from_cache") config.commands.read_from_cache = parse_bool(value);
        else if (key == "program_load") config.commands.program_load = parse_bool(value);
        else if (key == "program_execute") config.commands.program_execute = parse_bool(value);
        else if (key == "block_erase") config.commands.block_erase = parse_bool(value);
        return;
    }

    if (section == "registers") {
        if (key == "status1_default") config.registers.status1_default = parse_u8(value);
        else if (key == "status2_default") config.registers.status2_default = parse_u8(value);
        else if (key == "status3_default") config.registers.status3_default = parse_u8(value);
        else if (key == "feature_a0_default") config.registers.feature_a0_default = parse_u8(value);
        else if (key == "feature_b0_default") config.registers.feature_b0_default = parse_u8(value);
        else if (key == "feature_c0_default") config.registers.feature_c0_default = parse_u8(value);
        else if (key == "feature_d0_default") config.registers.feature_d0_default = parse_u8(value);
        else if (key == "feature_c0_dynamic") config.registers.feature_c0_dynamic = parse_bool(value);
        return;
    }

    if (section == "capabilities") {
        if (key == "quad_enable") config.capabilities.quad_enable = parse_bool(value);
        else if (key == "four_byte_address") config.capabilities.four_byte_address = parse_bool(value);
        else if (key == "block_protect") config.capabilities.block_protect = parse_bool(value);
        else if (key == "suspend_resume") config.capabilities.suspend_resume = parse_bool(value);
        else if (key == "otp") config.capabilities.otp = parse_bool(value);
        else if (key == "security_register") config.capabilities.security_register = parse_bool(value);
        else if (key == "ecc_status") config.capabilities.ecc_status = parse_bool(value);
        else if (key == "bad_block_management") config.capabilities.bad_block_management = parse_bool(value);
        else if (key == "copy_back") config.capabilities.copy_back = parse_bool(value);
        else if (key == "die_select") config.capabilities.die_select = parse_bool(value);
        else if (key == "plane_select") config.capabilities.plane_select = parse_bool(value);
        else if (key == "read_retry") config.capabilities.read_retry = parse_bool(value);
        return;
    }

    if (section == "constraints") {
        if (key == "require_wel") config.constraints.require_wel = parse_bool(value);
        else if (key == "require_qe_for_quad") config.constraints.require_qe_for_quad = parse_bool(value);
        else if (key == "default_all_protected") config.constraints.default_all_protected = parse_bool(value);
        else if (key == "strict_sequential_program") config.constraints.strict_sequential_program = parse_bool(value);
        else if (key == "max_partial_programs") config.constraints.max_partial_programs = parse_u32(value);
        else if (key == "ecc_reserved_start") config.constraints.ecc_reserved_start = parse_u32(value);
        else if (key == "ecc_reserved_end") config.constraints.ecc_reserved_end = parse_u32(value);
        else if (key == "nor_protect_start") config.constraints.nor_protect_start = parse_u64(value);
        else if (key == "nor_protect_length") config.constraints.nor_protect_length = parse_u64(value);
        return;
    }

    if (section == "policy") {
        if (key == "name") config.policy.name = unquote(value);
        return;
    }

    if (section == "policy.overrides") {
        config.policy.overrides[key] = unquote(value);
        return;
    }

    if (section == "source") {
        if (key == "datasheet") config.source.datasheet = unquote(value);
        else if (key == "page") config.source.page = unquote(value);
        else if (key == "confidence") config.source.confidence = parse_double(value);
        return;
    }
}

} // namespace

ModelConfig load_config_file(const std::string& path)
{
    std::ifstream input(path);
    if (!input) throw std::runtime_error("failed to open config: " + path);

    ModelConfig config;
    std::string section;
    std::string subsection;
    std::string line;
    int line_no = 0;

    while (std::getline(input, line)) {
        ++line_no;
        const std::size_t indent = leading_spaces(line);
        std::string clean = trim(strip_comment(line));
        if (clean.empty()) continue;

        if (clean.rfind("- ", 0) == 0) {
            if (section == "unsupported_features") {
                config.unsupported_features.push_back(unquote(clean.substr(2)));
            }
            continue;
        }

        const std::size_t colon = clean.find(':');
        if (colon == std::string::npos) {
            throw std::runtime_error("invalid config line " + std::to_string(line_no) + ": " + clean);
        }

        const std::string key = trim(clean.substr(0, colon));
        const std::string value = trim(clean.substr(colon + 1));

        if (indent == 0) {
            subsection.clear();
            if (value.empty()) {
                section = key;
                continue;
            }
            if (key == "schema_version") {
                config.schema_version = static_cast<int>(parse_u32(value));
                continue;
            }
            assign_value(config, "", key, value);
            continue;
        }

        if (indent == 2) {
            if (value.empty()) {
                subsection = section + "." + key;
                continue;
            }
            assign_value(config, section, key, value);
            continue;
        }

        if (indent == 4 && !subsection.empty()) {
            assign_value(config, subsection, key, value);
            continue;
        }
    }

    if (is_nand_like(config.device.cls) && config.geometry.page_size == 0) {
        config.geometry.page_size = config.geometry.main_size + config.geometry.spare_size;
    }
    if (!is_nand_like(config.device.cls) && config.geometry.block_size == 0) {
        config.geometry.block_size = config.geometry.sector_size;
    }

    return config;
}

} // namespace flash_model
