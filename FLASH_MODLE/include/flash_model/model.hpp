#ifndef FLASH_MODEL_MODEL_HPP
#define FLASH_MODEL_MODEL_HPP

#include "flash_model/address.hpp"
#include "flash_model/capability.hpp"
#include "flash_model/config.hpp"
#include "flash_model/interface.hpp"
#include "flash_model/policy.hpp"
#include "flash_model/registers.hpp"
#include "flash_model/storage.hpp"
#include "flash_model/timing.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace flash_model {

// OperationResult 是所有命令接口的统一返回结构，便于测试和上层工具解析。
struct OperationResult {
    bool ok = true; // 命令是否被接受或执行成功。
    std::string message; // 人类可读的执行结果或拒绝原因。
    std::vector<std::uint8_t> data; // 读命令返回的数据；写/擦命令通常为空。
    double now_us = 0.0; // 命令完成后模型内部的当前时间。
};

// FlashModel 是命令级外观层：它把配置、公共内核、能力模块和策略层组合起来。
class FlashModel {
public:
    FlashModel(ModelConfig config,
               std::unique_ptr<DevicePolicy> policy,
               std::vector<std::unique_ptr<CapabilityModule>> modules);

    FlashModel(FlashModel&&) noexcept = default;
    FlashModel& operator=(FlashModel&&) noexcept = default;
    FlashModel(const FlashModel&) = delete;
    FlashModel& operator=(const FlashModel&) = delete;

    // 只读观察接口：上层测试和 CLI 可以查看配置、运行状态和当前时间。
    const ModelConfig& config() const { return config_; }
    const RuntimeState& state() const { return registers_.state(); }
    double time_us() const { return timing_.now_us(); }
    bool busy();

    // 共有系统命令：NOR 和 NAND 都可能复用这些基础控制行为。
    OperationResult read_id();
    OperationResult write_enable();
    OperationResult write_disable();
    OperationResult wait_ready();
    OperationResult reset();
    OperationResult suspend();
    OperationResult resume();
    OperationResult read_unique_id();

    // NOR 专用系统能力：低功耗、4-byte 地址、SFDP。
    OperationResult deep_power_down();
    OperationResult release_power_down();
    OperationResult enter_4byte_address();
    OperationResult exit_4byte_address();
    OperationResult read_sfdp(std::uint32_t offset, std::size_t length);

    // NOR 主阵列命令：直接地址读写和 sector/block/chip erase。
    OperationResult nor_read(std::uint64_t address, std::size_t length);
    OperationResult nor_program(std::uint64_t address, const std::vector<std::uint8_t>& data);
    OperationResult nor_sector_erase(std::uint64_t address);
    OperationResult nor_block32_erase(std::uint64_t address);
    OperationResult nor_block_erase(std::uint64_t address);
    OperationResult nor_chip_erase();

    // NOR security register 命令：独立小区域，仍遵守 1->0 program 语义。
    OperationResult read_security_register(std::uint8_t index,
                                           std::uint16_t offset,
                                           std::size_t length);
    OperationResult erase_security_register(std::uint8_t index);
    OperationResult program_security_register(std::uint8_t index,
                                              std::uint16_t offset,
                                              const std::vector<std::uint8_t>& data);
    OperationResult lock_security_register(std::uint8_t index);

    // SPI-NAND feature/OTP/选择类命令。
    OperationResult enter_otp_mode();
    OperationResult exit_otp_mode();
    OperationResult get_feature(std::uint8_t address);
    OperationResult set_feature(std::uint8_t address, std::uint8_t value);
    OperationResult select_die(std::uint32_t die);
    OperationResult select_plane(std::uint32_t plane);
    OperationResult set_read_retry_level(std::uint32_t level);

    // SPI-NAND 主阵列命令：block erase、cache program、copy-back、page/cache read。
    OperationResult block_erase(std::uint32_t block);
    OperationResult program_load(std::uint16_t column,
                                 const std::vector<std::uint8_t>& data,
                                 bool random_load = false);
    OperationResult program_execute(std::uint32_t page);
    OperationResult copy_back(std::uint32_t source_page, std::uint32_t target_page);
    OperationResult page_read(std::uint32_t page);
    OperationResult read_from_cache(std::uint16_t column, std::size_t length);
    OperationResult read_parameter_page(std::uint16_t offset, std::size_t length);

private:
    ModelConfig config_; // 完整配置快照，模型运行期间不再依赖外部文件。
    std::unique_ptr<DevicePolicy> policy_; // 器件策略层，处理默认状态和少量族规则。
    std::vector<std::unique_ptr<CapabilityModule>> modules_; // 可选能力模块链。
    RegisterEngine registers_; // 状态寄存器、feature register、运行态标志。
    AddressMapper address_; // NOR/NAND 地址和块页映射。
    TimingEngine timing_; // 当前时间、busy 窗口、suspend/resume 时序。
    InterfaceEngine interface_; // 行为级接口传输耗时。
    SparseStorageBackend storage_; // 主阵列稀疏存储后端。

    std::vector<std::uint8_t> cache_; // SPI-NAND cache register。
    bool cache_valid_ = false; // cache 是否已被 page_read/program_load 填充。
    bool cache_program_violation_ = false; // cache 中是否写入 ECC reserved 等非法区域。
    std::map<std::uint32_t, std::uint32_t> partial_program_count_; // 普通 NAND 页编程次数。
    std::map<std::uint32_t, std::uint32_t> otp_program_count_; // OTP 页编程次数。
    std::map<std::uint32_t, std::uint32_t> next_page_by_block_; // 顺序编程约束的下一页。
    std::vector<std::uint8_t> security_; // NOR security register 存储区。
    std::vector<bool> security_locked_; // 每个 security register 的锁定位。
    SparseStorageBackend otp_storage_; // NAND OTP page 存储区。
    std::vector<std::uint8_t> sfdp_; // NOR SFDP 返回空间。
    std::vector<std::uint8_t> parameter_page_; // NAND parameter page 返回空间。

    OperationResult result(bool ok, const std::string& message,
                           const std::vector<std::uint8_t>& data = {});
    bool require_awake(const char* op) const;
    bool require_ready(const char* op);
    bool require_wel(const char* op);
    bool require_not_suspended(const char* op) const;

    // capability hook 聚合函数：FlashModel 只编排流程，具体拒绝规则交给模块。
    CapabilityDecision before_nor_program(std::uint64_t address, std::uint64_t length) const;
    CapabilityDecision before_nor_erase(std::uint64_t address, std::uint64_t length) const;
    CapabilityDecision before_nand_block_erase(std::uint32_t block) const;
    CapabilityDecision before_nand_program_execute(std::uint32_t page, std::uint32_t block) const;
    CapabilityDecision before_nand_copy_back(std::uint32_t source_page,
                                             std::uint32_t target_page,
                                             std::uint32_t target_block) const;
    bool marks_nand_program_load_violation(std::uint16_t column, std::uint64_t length) const;

    // NOR erase 共用执行路径：命令差异只保留在粒度、事务名和 busy 时长上。
    OperationResult execute_nor_erase(const char* op,
                                      bool command_enabled,
                                      std::uint64_t base,
                                      std::uint64_t length,
                                      const std::string& transaction_name,
                                      double busy_us,
                                      const std::string& accepted_message);

    // security register 与派生数据辅助函数。
    bool security_range_ok(std::uint8_t index, std::uint16_t offset, std::size_t length) const;
    std::uint64_t security_offset(std::uint8_t index, std::uint16_t offset) const;
    std::uint32_t security_register_count() const;
    std::uint32_t security_register_size() const;
    std::vector<std::uint8_t> generated_unique_id() const;
    void build_parameter_page();
    bool otp_page_ok(std::uint32_t page) const;

    // 命令事务与接口耗时辅助函数。
    std::uint32_t command_address_bytes() const;
    void account_interface(std::uint64_t request_bytes,
                           std::uint64_t response_bytes,
                           bool turnaround = true);
    void account_command(const std::string& command_name,
                         std::uint64_t write_payload_bytes = 0,
                         std::uint64_t read_response_bytes = 0);

    // SPI-NAND program_execute/copy_back 共用提交路径。
    OperationResult execute_cached_program(std::uint32_t page,
                                           const std::string& op,
                                           const std::string& accepted_message);
};

} // namespace flash_model

#endif
