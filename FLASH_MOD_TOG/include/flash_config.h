#ifndef FLASH_MOD_FLASH_CONFIG_H
#define FLASH_MOD_FLASH_CONFIG_H

#include <cstdint>
#include <string>
#include <vector>

namespace flashmod {

// FlashClass 是第一层器件分类选择。当前 FlashCore 已实现 NOR 和
// SPI-NAND 命令路径；RAW_NAND 先作为后续扩展分类保留，这样配置模板
// 和上层调用接口不用因为新增 NAND 形态而整体重写。
enum class FlashClass {
    NOR,
    SPI_NAND,
    RAW_NAND
};

std::string to_string(FlashClass cls);
FlashClass flash_class_from_string(const std::string& text);

// 时序统一使用 us 和 MHz：内部 busy 时间用 us，串行总线速率用 MHz。
// 配置文件可以来自数据手册的 typical/max 参数；FlashCore 再根据
// use_max_time 决定使用典型值还是最大值。
struct FlashTimings {
    // 串行总线时钟域：opcode/address 使用 f_cmd_mhz，普通数据使用
    // f_read_mhz，x4/Quad/DTR 数据路径使用 f_quad_mhz。
    double f_cmd_mhz = 100.0;
    double f_read_mhz = 50.0;
    double f_quad_mhz = 100.0;

    // 上电、读页、编程、擦除和复位时序。NOR 使用 sector/32K/64K/chip
    // 擦除时间；SPI-NAND 使用 t_block_erase_us。
    double t_puw_us = 0.0;
    double t_read_us = 25.0;
    double t_read_ecc_on_us = 50.0;
    double t_prog_us = 300.0;
    double t_prog_max_us = 1000.0;
    double t_sector_erase_us = 45000.0;
    double t_block32_erase_us = 120000.0;
    double t_block64_erase_us = 150000.0;
    double t_block_erase_us = 2000.0;
    double t_block_erase_max_us = 10000.0;
    double t_chip_erase_us = 10000000.0;
    double t_reset_us = 30.0;
};

// DeviceProfile 是 config_parser 输出、模型层消费的唯一器件画像。目标是：
// 上层工具从数据手册提取参数并写入配置文件，普通 NOR/SPI-NAND 差异不再
// 通过复制一份 C++ 芯片模型来表达。
struct DeviceProfile {
    // 来源信息。config_dir 用来让相对 STORAGE_FILE 路径解析到配置文件所在目录。
    std::string source_path;
    std::string config_dir;
    std::string chip_name = "GENERIC_FLASH";
    std::string storage_file = "flash_mod_storage.bin";
    FlashClass flash_class = FlashClass::NOR;

    // 几何结构。NOR 通常由 memory_size + page/sector/block size 描述；
    // SPI-NAND 通常由 main_size + spare_size、pages_per_block、blocks、planes
    // 描述。解析结束后 derive_profile() 会补齐可推导字段。
    std::uint64_t memory_size = 0;
    std::uint32_t page_size = 256;
    std::uint32_t main_size = 0;
    std::uint32_t spare_size = 0;
    std::uint32_t sector_size = 4096;
    std::uint32_t block32_size = 32768;
    std::uint32_t block64_size = 65536;
    std::uint32_t pages_per_block = 64;
    std::uint32_t blocks = 0;
    std::uint32_t planes = 1;
    std::uint32_t blocks_per_plane = 0;
    std::uint32_t address_bytes = 3;

    // 仿真策略开关。这些不是物理寄存器，而是控制模型严格程度和 storage 持久化方式。
    bool use_max_time = false;
    bool auto_complete = false;
    bool wrap_address = true;
    bool reset_storage_on_start = false;

    // 通用命令行为开关。
    bool page_program_wrap = true;
    bool program_load_requires_wel = true;
    bool require_wel = true;
    bool require_qe_for_quad = true;
    bool quad_enable_default = false;
    bool deep_power_down_supported = true;
    bool reset_supported = true;

    // SPI-NAND ECC 相关配置。ECC 开启时，写到 reserved spare 区可被拒绝，
    // 用来模拟部分厂商对内部 ECC 区域的限制。
    bool ecc_supported = false;
    bool ecc_enabled_default = false;
    std::uint32_t ecc_reserved_start = 0;
    std::uint32_t ecc_reserved_end = 0;
    bool generate_internal_ecc = false;

    // NAND 保护、坏块和编程顺序策略。
    bool default_all_protected = false;
    bool strict_sequential_program = false;
    bool strict_bad_block = false;
    std::uint32_t bad_block_marker_column = 0;
    std::uint32_t max_partial_programs = 4;
    std::uint32_t max_bbm_entries = 0;

    // 可选特殊区域。先放进画像里，后续补 OTP/security-register 命令时不用再改配置框架。
    bool security_registers = false;
    std::uint32_t security_register_count = 0;
    std::uint32_t security_register_size = 256;
    bool otp_supported = false;
    std::uint32_t otp_pages = 0;
    std::uint32_t otp_size = 0;

    // 上电后的状态寄存器和 SPI-NAND feature register 默认值。
    std::uint8_t status1_default = 0;
    std::uint8_t status2_default = 0;
    std::uint8_t status3_default = 0;
    std::uint8_t feature_a0_default = 0;
    std::uint8_t feature_b0_default = 0;
    std::uint8_t feature_d0_default = 0;
    std::uint8_t feature_f0_default = 0;

    std::vector<std::uint8_t> id_bytes;
    FlashTimings timings;

    // AddressMapper 和 FlashCore 使用的派生几何接口，避免几何公式散落在各个模块里。
    std::uint32_t total_pages() const;
    std::uint64_t total_size_bytes() const;
    std::uint32_t effective_page_size() const;
    std::uint32_t effective_blocks() const;
    bool is_nand_like() const;
};

} // namespace flashmod

#endif
