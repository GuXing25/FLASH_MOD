#include "config_parser.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace flashmod {
namespace {

std::string trim(const std::string& text)
{
    // 配置文件允许缩进和行尾空白；解析前统一去掉。
    std::size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first]))) ++first;
    std::size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1]))) --last;
    return text.substr(first, last - first);
}

std::string upper(std::string text)
{
    // KEY 不区分大小写，统一转大写后匹配别名。
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return text;
}

std::vector<std::string> split_values(std::string text)
{
    // ID_BYTES/JEDEC_ID 常见写法可能用逗号、冒号、分号或等号分隔，这里统一成空格。
    for (char& ch : text) {
        if (ch == ',' || ch == ';' || ch == ':' || ch == '=') ch = ' ';
    }
    std::istringstream in(text);
    std::vector<std::string> out;
    std::string token;
    while (in >> token) out.push_back(token);
    return out;
}

std::uint64_t parse_u64(const std::string& text)
{
    // base=0 允许 123、0x7B 等 C 风格整数格式。
    std::size_t pos = 0;
    std::uint64_t value = std::stoull(text, &pos, 0);
    if (pos != text.size()) throw std::runtime_error("invalid integer: " + text);
    return value;
}

double parse_double(const std::string& text)
{
    std::size_t pos = 0;
    double value = std::stod(text, &pos);
    if (pos != text.size()) throw std::runtime_error("invalid number: " + text);
    return value;
}

bool parse_bool(const std::string& text)
{
    // 兼容配置文件里常见的布尔写法。
    const std::string v = upper(text);
    if (v == "1" || v == "TRUE" || v == "YES" || v == "ON") return true;
    if (v == "0" || v == "FALSE" || v == "NO" || v == "OFF") return false;
    throw std::runtime_error("invalid bool: " + text);
}

std::vector<std::uint8_t> parse_bytes(const std::string& text)
{
    std::vector<std::uint8_t> out;
    for (const std::string& token : split_values(text)) {
        // 两位十六进制如 "EF" 自动按 0xEF 解析；带 0x 的写法也支持。
        std::uint64_t value = parse_u64(token.size() == 2 ? "0x" + token : token);
        if (value > 0xFFu) throw std::runtime_error("byte value out of range: " + token);
        out.push_back(static_cast<std::uint8_t>(value));
    }
    return out;
}

std::uint32_t u32(const std::string& value)
{
    return static_cast<std::uint32_t>(parse_u64(value));
}

void derive_profile(DeviceProfile& p)
{
    // derive_profile 是“配置模板”和“模型内部需要字段”之间的缓冲层：
    // 配置文件可以只给数据手册里最自然的字段，这里再补出等价派生值。
    if (!p.is_nand_like()) {
        throw std::runtime_error("NAND-only project only accepts SPI_NAND/RAW_NAND profiles");
    }

    if (p.is_nand_like()) {
        // SPI-NAND 页大小通常是 main+spare。若 PAGE_SIZE 没被显式覆盖，就按两者相加。
        if (p.page_size == 256 && p.main_size > 0) p.page_size = p.main_size + p.spare_size;
        if (p.main_size == 0) p.main_size = p.page_size > p.spare_size ? p.page_size - p.spare_size : p.page_size;
        // blocks 可以直接给，也可以由 blocks_per_plane * planes 推导。
        if (p.blocks == 0 && p.blocks_per_plane != 0) p.blocks = p.blocks_per_plane * std::max<std::uint32_t>(p.planes, 1);
        if (p.memory_size == 0 && p.blocks != 0 && p.pages_per_block != 0) {
            p.memory_size = static_cast<std::uint64_t>(p.blocks) * p.pages_per_block * p.effective_page_size();
        }
        // 把高层布尔默认值投影到 SPI-NAND feature register 默认位。
        if (p.ecc_supported && p.ecc_enabled_default) p.feature_b0_default |= 0x10u;
        if (p.quad_enable_default) p.feature_b0_default |= 0x01u;
        if (p.default_all_protected && p.feature_a0_default == 0) p.feature_a0_default = 0x7Cu;
        if (p.max_partial_programs == 0) p.max_partial_programs = 1;
        if (p.ecc_reserved_end != 0 && p.ecc_reserved_end < p.ecc_reserved_start) {
            throw std::runtime_error("ECC_RESERVED_END must be >= ECC_RESERVED_START");
        }
    } else {
        // NOR 没有 block/page 元数据，必须知道线性阵列大小。
        if (p.memory_size == 0) throw std::runtime_error("NOR profile requires MEMORY_SIZE");
        // SPI NOR 常见 QE 位在 SR2 bit1。
        if (p.quad_enable_default) p.status2_default |= 0x02u;
        if (p.block64_size == 0) p.block64_size = p.sector_size;
        if (p.block32_size == 0) p.block32_size = p.sector_size;
    }

    if (p.id_bytes.empty()) p.id_bytes = {0xFF, 0xFF, 0xFF};
    if (p.storage_file.empty()) p.storage_file = "flash_mod_storage.bin";
    // page_size 是地址映射和 NOR 页编程的基础，不能为 0。
    if (p.page_size == 0) throw std::runtime_error("PAGE_SIZE must be non-zero");
}

} // namespace

std::string to_string(FlashClass cls)
{
    switch (cls) {
    case FlashClass::NOR: return "NOR";
    case FlashClass::SPI_NAND: return "SPI_NAND";
    case FlashClass::RAW_NAND: return "RAW_NAND";
    }
    return "UNKNOWN";
}

FlashClass flash_class_from_string(const std::string& text)
{
    const std::string v = upper(text);
    if (v == "SPI_NAND" || v == "SPINAND") return FlashClass::SPI_NAND;
    if (v == "RAW_NAND" || v == "NAND") return FlashClass::RAW_NAND;
    throw std::runtime_error("NAND-only project does not support FLASH_CLASS: " + text);
}

std::uint32_t DeviceProfile::total_pages() const
{
    if (is_nand_like()) return effective_blocks() * pages_per_block;
    return page_size == 0 ? 0 : static_cast<std::uint32_t>(memory_size / page_size);
}

std::uint64_t DeviceProfile::total_size_bytes() const
{
    if (memory_size != 0) return memory_size;
    if (is_nand_like()) {
        return static_cast<std::uint64_t>(effective_blocks()) * pages_per_block * effective_page_size();
    }
    return 0;
}

std::uint32_t DeviceProfile::effective_page_size() const
{
    return page_size != 0 ? page_size : main_size + spare_size;
}

std::uint32_t DeviceProfile::effective_blocks() const
{
    if (blocks != 0) return blocks;
    return blocks_per_plane * std::max<std::uint32_t>(planes, 1);
}

bool DeviceProfile::is_nand_like() const
{
    return flash_class == FlashClass::SPI_NAND || flash_class == FlashClass::RAW_NAND;
}

DeviceProfile load_profile(const std::string& filename)
{
    std::ifstream in(filename);
    if (!in) throw std::runtime_error("cannot open config: " + filename);

    // 记录配置文件绝对路径，后续解析相对 storage_file 时使用。
    DeviceProfile p;
    std::filesystem::path path(filename);
    p.source_path = std::filesystem::absolute(path).string();
    p.config_dir = std::filesystem::absolute(path).parent_path().string();

    std::string line;
    int lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        // # 之后视为注释，空行直接跳过。
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) line = line.substr(0, comment);
        line = trim(line);
        if (line.empty()) continue;

        std::replace(line.begin(), line.end(), '=', ' ');
        std::istringstream ss(line);
        std::string key;
        ss >> key;
        std::string rest;
        std::getline(ss, rest);
        rest = trim(rest);
        if (rest.empty()) continue;

        key = upper(key);
        try {
            // 字段匹配使用别名集合，便于承接不同数据手册或旧模型中的命名习惯。
            if (key == "FLASH_CLASS" || key == "CLASS" || key == "TYPE") p.flash_class = flash_class_from_string(rest);
            else if (key == "CHIP_NAME") p.chip_name = rest;
            else if (key == "STORAGE_FILE") p.storage_file = rest;
            else if (key == "MEMORY_SIZE") p.memory_size = parse_u64(rest);
            else if (key == "PAGE_SIZE") p.page_size = u32(rest);
            else if (key == "MAIN_SIZE" || key == "PAGE_MAIN_SIZE") p.main_size = u32(rest);
            else if (key == "SPARE_SIZE" || key == "PAGE_SPARE_SIZE") p.spare_size = u32(rest);
            else if (key == "SECTOR_SIZE") p.sector_size = u32(rest);
            else if (key == "BLOCK32_SIZE") p.block32_size = u32(rest);
            else if (key == "BLOCK64_SIZE") p.block64_size = u32(rest);
            else if (key == "PAGES_PER_BLOCK" || key == "PAGE_PER_BLOCK") p.pages_per_block = u32(rest);
            else if (key == "BLOCKS" || key == "BLOCK_COUNT" || key == "TOTAL_BLOCKS") p.blocks = u32(rest);
            else if (key == "PLANES" || key == "PLANE_COUNT") p.planes = u32(rest);
            else if (key == "BLOCKS_PER_PLANE" || key == "BLOCK_PER_PLANE") p.blocks_per_plane = u32(rest);
            else if (key == "ADDRESS_BYTES") p.address_bytes = u32(rest);

            else if (key == "USE_MAX_TIME" || key == "USE_MAX_TIMING") p.use_max_time = parse_bool(rest);
            else if (key == "AUTO_COMPLETE") p.auto_complete = parse_bool(rest);
            else if (key == "WRAP_ADDRESS") p.wrap_address = parse_bool(rest);
            else if (key == "RESET_STORAGE_ON_START") p.reset_storage_on_start = parse_bool(rest);
            else if (key == "PAGE_PROGRAM_WRAP") p.page_program_wrap = parse_bool(rest);
            else if (key == "PROGRAM_LOAD_REQUIRES_WEL") p.program_load_requires_wel = parse_bool(rest);
            else if (key == "REQUIRE_WEL") p.require_wel = parse_bool(rest);
            else if (key == "REQUIRE_QE_FOR_QUAD") p.require_qe_for_quad = parse_bool(rest);
            else if (key == "QUAD_ENABLE_DEFAULT" || key == "QE_DEFAULT") p.quad_enable_default = parse_bool(rest);
            else if (key == "DEEP_POWER_DOWN_SUPPORTED") p.deep_power_down_supported = parse_bool(rest);
            else if (key == "RESET_SUPPORTED") p.reset_supported = parse_bool(rest);

            else if (key == "ECC_SUPPORTED") p.ecc_supported = parse_bool(rest);
            else if (key == "ECC_ENABLE" || key == "ECC_ENABLED_DEFAULT") p.ecc_enabled_default = parse_bool(rest);
            else if (key == "ECC_RESERVED_START") p.ecc_reserved_start = u32(rest);
            else if (key == "ECC_RESERVED_END") p.ecc_reserved_end = u32(rest);
            else if (key == "GENERATE_INTERNAL_ECC_PARITY") p.generate_internal_ecc = parse_bool(rest);

            else if (key == "DEFAULT_ALL_PROTECTED" || key == "DEFAULT_PROTECTED") p.default_all_protected = parse_bool(rest);
            else if (key == "STRICT_SEQUENTIAL_PROGRAM") p.strict_sequential_program = parse_bool(rest);
            else if (key == "STRICT_BAD_BLOCK") p.strict_bad_block = parse_bool(rest);
            else if (key == "BAD_BLOCK_MARKER_COLUMN" || key == "BAD_BLOCK_MARK_OFFSET") p.bad_block_marker_column = u32(rest);
            else if (key == "MAX_PARTIAL_PROGRAMS" || key == "MAX_PROGRAMS_PER_PAGE") p.max_partial_programs = u32(rest);
            else if (key == "MAX_BBM_ENTRIES") p.max_bbm_entries = u32(rest);

            else if (key == "HAS_SECURITY_REGISTERS" || key == "SECURITY_REGISTERS") p.security_registers = parse_bool(rest);
            else if (key == "SECURITY_REGISTER_COUNT") p.security_register_count = u32(rest);
            else if (key == "SECURITY_REGISTER_SIZE") p.security_register_size = u32(rest);
            else if (key == "OTP_SUPPORTED") p.otp_supported = parse_bool(rest);
            else if (key == "OTP_PAGES") p.otp_pages = u32(rest);
            else if (key == "OTP_SIZE") p.otp_size = u32(rest);

            else if (key == "STATUS1_DEFAULT" || key == "SR1_DEFAULT") p.status1_default = static_cast<std::uint8_t>(parse_u64(rest));
            else if (key == "STATUS2_DEFAULT" || key == "SR2_DEFAULT") p.status2_default = static_cast<std::uint8_t>(parse_u64(rest));
            else if (key == "STATUS3_DEFAULT" || key == "SR3_DEFAULT") p.status3_default = static_cast<std::uint8_t>(parse_u64(rest));
            else if (key == "FEATURE_A0_DEFAULT" || key == "BLOCK_LOCK_DEFAULT") p.feature_a0_default = static_cast<std::uint8_t>(parse_u64(rest));
            else if (key == "FEATURE_B0_DEFAULT" || key == "CONFIG_DEFAULT") p.feature_b0_default = static_cast<std::uint8_t>(parse_u64(rest));
            else if (key == "FEATURE_D0_DEFAULT" || key == "DIE_SELECT_DEFAULT") p.feature_d0_default = static_cast<std::uint8_t>(parse_u64(rest));
            else if (key == "FEATURE_F0_DEFAULT") p.feature_f0_default = static_cast<std::uint8_t>(parse_u64(rest));
            else if (key == "JEDEC_ID" || key == "ID_BYTES") p.id_bytes = parse_bytes(rest);

            else if (key == "F_CLOCK_MHZ" || key == "F_C_MHZ" || key == "F_SPI_MHZ") p.timings.f_cmd_mhz = parse_double(rest);
            else if (key == "F_READ_MHZ" || key == "F_R_MHZ") p.timings.f_read_mhz = parse_double(rest);
            else if (key == "F_QUAD_MHZ" || key == "F_Q_MHZ" || key == "F_DUAL_QUAD_IO_MHZ") p.timings.f_quad_mhz = parse_double(rest);
            else if (key == "T_PUW_US") p.timings.t_puw_us = parse_double(rest);
            else if (key == "T_READ_US" || key == "T_READ" || key == "T_READ_ECC_OFF_US" || key == "T_RD_US") p.timings.t_read_us = parse_double(rest);
            else if (key == "T_READ_ECC_ON_US" || key == "T_RD_ECC_TYP_US") p.timings.t_read_ecc_on_us = parse_double(rest);
            else if (key == "T_PROG_US" || key == "T_PP_US" || key == "T_PROG" || key == "T_PROG_TYP_US") p.timings.t_prog_us = parse_double(rest);
            else if (key == "T_PROG_MAX_US" || key == "T_PP_MAX_US") p.timings.t_prog_max_us = parse_double(rest);
            else if (key == "T_SE_US" || key == "T_ERASE_SECTOR_US") p.timings.t_sector_erase_us = parse_double(rest);
            else if (key == "T_BE32_US") p.timings.t_block32_erase_us = parse_double(rest);
            else if (key == "T_BE64_US") p.timings.t_block64_erase_us = parse_double(rest);
            else if (key == "T_ERASE_US" || key == "T_BERS_TYP_US" || key == "T_ERASE") p.timings.t_block_erase_us = parse_double(rest);
            else if (key == "T_ERASE_MAX_US" || key == "T_BERS_MAX_US") p.timings.t_block_erase_max_us = parse_double(rest);
            else if (key == "T_CE_US") p.timings.t_chip_erase_us = parse_double(rest);
            else if (key == "T_RESET_US" || key == "T_RST_US") p.timings.t_reset_us = parse_double(rest);
            else {
                // 未识别字段不直接报错，是为了允许配置先承载未来模型会用到的参数。
                std::cerr << "[配置] 忽略暂未使用字段 " << key << "\n";
            }
        } catch (const std::exception& e) {
            // 给错误补上文件名和行号，便于定位配置字段。
            std::ostringstream os;
            os << filename << ":" << lineno << ": " << e.what();
            throw std::runtime_error(os.str());
        }
    }

    derive_profile(p);
    return p;
}

void print_profile(const DeviceProfile& p)
{
    std::cout << "=== FLASH_MOD Profile ===\n";
    std::cout << "chip_name: " << p.chip_name << "\n";
    std::cout << "class: " << to_string(p.flash_class) << "\n";
    std::cout << "storage_file: " << p.storage_file << "\n";
    std::cout << "total_size: " << p.total_size_bytes() << " bytes\n";
    std::cout << "page_size: " << p.effective_page_size() << "\n";
    if (p.is_nand_like()) {
        std::cout << "main/spare: " << p.main_size << "/" << p.spare_size << "\n";
        std::cout << "blocks/pages_per_block: " << p.effective_blocks() << "/" << p.pages_per_block << "\n";
        std::cout << "ecc_default: " << (p.ecc_enabled_default ? 1 : 0) << "\n";
    } else {
        std::cout << "sector/block32/block64: " << p.sector_size << "/"
                  << p.block32_size << "/" << p.block64_size << "\n";
    }
    std::cout << "id:";
    for (std::uint8_t b : p.id_bytes) {
        std::cout << " " << std::hex << std::uppercase << std::setw(2)
                  << std::setfill('0') << static_cast<int>(b);
    }
    std::cout << std::dec << std::nouppercase << std::setfill(' ') << "\n";
}

} // namespace flashmod
