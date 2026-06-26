#ifndef FLASH_MODEL_CONFIG_HPP
#define FLASH_MODEL_CONFIG_HPP

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace flash_model {

// FlashClass 是模型的一级器件类别，用来决定后续走 NOR 还是 NAND 风格的行为路径。
enum class FlashClass {
    Nor,      // SPI NOR / parallel NOR 风格：直接地址读写、sector/block/chip erase。
    SpiNand, // SPI-NAND 风格：page read 到 cache，再从 cache 读出。
    RawNand  // 预留给裸 NAND 或更接近 ONFI 的 NAND 建模。
};

std::string to_string(FlashClass cls);
FlashClass flash_class_from_string(const std::string& text);
bool is_nand_like(FlashClass cls);

// Evidence 记录某个配置值来自哪里，便于回溯 datasheet 页码和 AI 提取置信度。
struct Evidence {
    std::string datasheet; // 数据手册文件名或资料来源名。
    std::string page;      // 参数所在页码、章节或表格编号。
    double confidence = 0.0; // 外部 AI 对该字段提取结果的置信度。
};

// DeviceConfig 描述器件身份信息，主要对应 datasheet 首页、ID 表和订购型号表。
struct DeviceConfig {
    std::string name;         // 模型展示名，例如 W25Q32JV。
    FlashClass cls = FlashClass::Nor; // 器件大类，决定 validator 和 model 的主分支。
    std::string manufacturer; // 厂商名，例如 Winbond / Micron / GigaDevice。
    std::string part_number;  // 完整订购型号，允许比 name 更具体。
    std::vector<std::uint8_t> id; // READ ID 返回值。
    std::vector<std::uint8_t> unique_id; // 可选唯一 ID；为空时模型会生成稳定伪 ID。
};

// GeometryConfig 描述阵列组织方式；NOR 与 NAND 使用不同字段组。
struct GeometryConfig {
    std::uint64_t memory_size = 0; // NOR 总容量，单位 byte。
    std::uint32_t page_size = 0;   // NOR page program 的页大小，单位 byte。
    std::uint32_t sector_size = 0; // NOR sector erase 粒度，通常 4KiB。
    std::uint32_t block32_size = 0; // NOR 32KiB block erase 粒度。
    std::uint32_t block_size = 0;   // NOR 64KiB 或更大 block erase 粒度。
    std::uint32_t main_size = 0;    // NAND 主区大小，单位 byte。
    std::uint32_t spare_size = 0;   // NAND spare/OOB 区大小，单位 byte。
    std::uint32_t pages_per_block = 0; // NAND 每个 block 的 page 数。
    std::uint32_t blocks = 0;          // NAND block 总数。
    std::uint32_t planes = 1;          // NAND plane 数；未建模 plane 时保持 1。
    std::uint32_t dies = 1;            // NAND die 数；未建模 die select 时保持 1。
};

// TimingConfig 存行为级时间，不追求 cycle accurate，但要覆盖 busy/wait-ready。
struct TimingConfig {
    double read_us = 0.0;          // 普通读、cache 读或寄存器读的抽象耗时。
    double page_read_us = 0.0;     // SPI-NAND page read 从阵列到 cache 的耗时。
    double program_us = 0.0;       // page program / program execute 的 busy 时长。
    double sector_erase_us = 0.0;  // NOR sector erase 或小粒度 erase 时长。
    double block32_erase_us = 0.0; // NOR 32KiB block erase 时长。
    double block_erase_us = 0.0;   // NOR/NAND block erase 时长。
    double chip_erase_us = 0.0;    // NOR chip erase 时长。
    double reset_us = 0.0;         // reset 命令后的恢复耗时。
    double puw_us = 0.0;           // power-up wait；当前作为配置保留字段。
};

// InterfaceConfig 是行为级芯粒/控制器接口模型，只描述传输耗时，不描述电气细节。
struct InterfaceConfig {
    bool enabled = false;                // 是否启用接口传输时间叠加。
    std::string name = "disabled";       // 接口实例名，用于日志或 profile 说明。
    std::string protocol = "none";       // 当前 validator 接受 none/disabled/generic_chiplet。
    std::uint32_t lanes = 1;             // 逻辑 lane 数，用于估算带宽。
    std::uint32_t data_width_bits = 32;  // 每 lane 或抽象链路宽度，单位 bit。
    double clock_mhz = 0.0;              // 接口时钟频率，单位 MHz。
    double fixed_latency_us = 0.0;       // 每次 transaction 固定延迟。
    double turnaround_us = 0.0;          // 从写相位转读相位的抽象 turnaround。
    std::uint32_t packet_overhead_bytes = 0; // 每个 packet 的头部/封装额外字节。
    std::uint32_t max_payload_bytes = 0;     // 单包最大 payload；0 表示不分包。
};

// TransactionConfig 描述单条命令的传输形状，用于把 opcode/address/data 转为接口耗时。
struct TransactionConfig {
    std::uint32_t opcode_value = 0; // 命令 opcode 数值，仅用于记录和校验。
    bool has_opcode_value = false;  // 区分 opcode_value=0 和未提供 opcode。
    std::uint32_t opcode_bytes = 1; // opcode 字节数，常见 SPI 命令为 1。
    std::uint32_t address_bytes = 0; // 固定地址字节数。
    std::uint32_t dummy_bytes = 0;   // dummy 字节数；用于当前 byte-based timing。
    std::uint32_t dummy_cycles = 0;  // dummy 周期数；先记录，后续可做 lane/cycle 细化。
    std::uint32_t fixed_request_bytes = 0;  // 除 opcode/address/dummy 外的固定请求字节。
    std::uint32_t fixed_response_bytes = 0; // 固定响应字节数。
    std::uint32_t command_lanes = 1; // opcode 相位 lane 数。
    std::uint32_t address_lanes = 1; // address 相位 lane 数。
    std::uint32_t data_lanes = 1;    // data 相位 lane 数。
    bool use_current_address_bytes = false; // 是否使用运行时 3-byte/4-byte 地址模式。
    bool write_payload = false;      // 是否把调用方传入的数据长度加入 request。
    bool read_response = false;      // 是否把调用方期望读出的长度加入 response。
    bool turnaround = true;          // 是否需要写转读 turnaround 时间。
    std::string alias_of;            // 命令别名，例如 fast_read 复用 nor_read 形状。
};

// CommandConfig 只描述“命令是否存在”，命令细节由 transaction/capability/policy 补充。
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

// RegisterConfig 放上电默认寄存器值；动态位仍由 RegisterEngine 在运行时合成。
struct RegisterConfig {
    std::uint8_t status1_default = 0;
    std::uint8_t status2_default = 0;
    std::uint8_t status3_default = 0;
    std::uint8_t feature_a0_default = 0;
    std::uint8_t feature_b0_default = 0;
    std::uint8_t feature_c0_default = 0;
    std::uint8_t feature_d0_default = 0;
    bool feature_c0_dynamic = false; // SPI-NAND C0h 常用作动态状态寄存器。
};

// CapabilityConfig 描述可选能力开关；复杂行为由对应 CapabilityModule 实现。
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

// ConstraintConfig 放通用参数化约束；如果规则过于私有，应升级到 DevicePolicy。
struct ConstraintConfig {
    bool require_wel = true; // 写、擦命令是否必须先执行 write_enable。
    bool require_qe_for_quad = true; // quad 操作是否需要 QE 位；当前作为约束保留。
    bool default_all_protected = false; // NAND block protect 上电是否默认全保护。
    bool strict_sequential_program = false; // NAND 是否要求 block 内顺序编程。
    std::uint32_t max_partial_programs = 4; // NAND 同一页最大 partial program 次数。
    std::uint32_t ecc_reserved_start = 0;   // ECC 保护区起始 column。
    std::uint32_t ecc_reserved_end = 0;     // ECC 保护区结束 column，半开区间。
    std::vector<std::uint32_t> initial_bad_blocks; // 上电即存在的坏块列表。
    std::uint64_t nor_protect_start = 0;    // NOR 保护窗口起始地址。
    std::uint64_t nor_protect_length = 0;   // NOR 保护窗口长度。
    std::uint32_t address_bytes = 3;        // SPI NOR 上电地址宽度，通常 3 或 4。
    std::uint32_t security_register_count = 3; // NOR security register 个数。
    std::uint32_t security_register_size = 256; // 单个 security register 大小。
    std::uint32_t otp_page_count = 0;       // NAND OTP page 数。
    std::uint32_t read_retry_levels = 0;    // NAND read-retry 档位数。
    std::uint32_t unique_id_size = 8;       // unique ID 长度。
    std::uint32_t sfdp_size = 256;          // SFDP 空间大小。
    std::uint32_t parameter_page_size = 256; // NAND parameter page 大小。
    std::uint32_t bad_block_marker_offset = 0; // 坏块标记在 page/OOB 中的偏移。
    std::uint32_t minimum_valid_blocks = 0; // datasheet 给出的最小有效块数。
    std::uint32_t ecc_bits = 0;             // ECC 可纠错 bit 数。
};

// PolicyConfig 选择器件策略层；overrides 预留给少量字符串形式私有规则。
struct PolicyConfig {
    std::string name; // generic_nor / generic_spinand / winbond_family 等策略名。
    std::map<std::string, std::string> overrides; // 策略私有覆盖项。
};

// ModelConfig 是外部 AI 输出配置进入框架后的完整内存结构。
struct ModelConfig {
    int schema_version = 1; // schema 版本，用于未来兼容升级。
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
    Evidence source; // profile 总体来源。
    std::map<std::string, Evidence> field_evidence; // 字段级证据索引。
    std::vector<std::string> unsupported_features; // 已知但暂不支持的 datasheet 能力。

    std::uint32_t effective_page_size() const; // NOR 返回 page_size，NAND 返回 main+spare。
    std::uint64_t total_size_bytes() const;    // 返回整个存储阵列大小。
    bool has_evidence(const std::string& field) const; // 判断字段是否带来源证据。
};

} // namespace flash_model

#endif
