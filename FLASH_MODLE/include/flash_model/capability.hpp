#ifndef FLASH_MODEL_CAPABILITY_HPP
#define FLASH_MODEL_CAPABILITY_HPP

#include "flash_model/config.hpp"
#include "flash_model/validator.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace flash_model {

// RuntimeState 是模型运行期状态快照，供 capability module 和 policy 共同读取/更新。
struct RuntimeState {
    std::uint8_t status1 = 0; // NOR status1 或通用 status byte。
    std::uint8_t status2 = 0; // NOR status2，常承载 QE/保护位等。
    std::uint8_t status3 = 0; // NOR status3 或厂商扩展状态位。
    std::map<std::uint8_t, std::uint8_t> features; // SPI-NAND feature register 映射。
    bool quad_enabled = false; // 是否已启用 quad mode。
    bool deep_power_down = false; // NOR 是否处于 deep power-down。
    bool suspended = false; // 当前是否 suspend 了一个 busy 操作。
    bool otp_mode = false; // SPI-NAND 是否进入 OTP page 访问模式。
    std::uint32_t address_bytes = 3; // 运行时地址宽度，支持 3-byte/4-byte 切换。
    std::uint32_t active_die = 0; // 当前 die select 状态。
    std::uint32_t active_plane = 0; // 当前 plane select 状态。
    std::uint32_t read_retry_level = 0; // 当前 read-retry 档位。
    std::vector<std::string> enabled_capabilities; // 已挂载能力列表，便于 CLI 展示。
    std::vector<std::uint32_t> bad_blocks; // 运行期已知坏块列表。
};

// CapabilityDecision 是 hook 的统一裁决结果。
struct CapabilityDecision {
    bool ok = true; // true 表示允许命令继续执行。
    std::string message; // false 时给出拒绝原因。

    static CapabilityDecision allow() { return CapabilityDecision{}; }
    static CapabilityDecision reject(const std::string& reason)
    {
        CapabilityDecision decision;
        decision.ok = false;
        decision.message = reason;
        return decision;
    }
};

// CapabilityModule 是可选能力的扩展点。
// 默认 hook 均放行，具体模块只覆盖自己关心的命令前置检查。
class CapabilityModule {
public:
    virtual ~CapabilityModule() = default;

    virtual std::string name() const = 0; // 能力名，与 CapabilityConfig 字段保持一致。
    virtual void validate(const ModelConfig& config, ValidationReport& report) const = 0;
    virtual void attach(RuntimeState& state) const = 0;

    // NOR program 前置检查，例如 block protect。
    virtual CapabilityDecision before_nor_program(const ModelConfig& config,
                                                  const RuntimeState& state,
                                                  std::uint64_t address,
                                                  std::uint64_t length) const;

    // NOR erase 前置检查，例如保护区拒绝擦除。
    virtual CapabilityDecision before_nor_erase(const ModelConfig& config,
                                                const RuntimeState& state,
                                                std::uint64_t address,
                                                std::uint64_t length) const;

    // NAND block erase 前置检查，例如 block protect 或 bad block。
    virtual CapabilityDecision before_nand_block_erase(const ModelConfig& config,
                                                       const RuntimeState& state,
                                                       std::uint32_t block) const;

    // NAND program execute 前置检查，例如 ECC reserved、partial program、bad block。
    virtual CapabilityDecision before_nand_program_execute(const ModelConfig& config,
                                                           const RuntimeState& state,
                                                           std::uint32_t page,
                                                           std::uint32_t block,
                                                           bool cached_program_violation) const;

    // NAND copy-back 前置检查，例如禁止同页 copy-back。
    virtual CapabilityDecision before_nand_copy_back(const ModelConfig& config,
                                                     const RuntimeState& state,
                                                     std::uint32_t source_page,
                                                     std::uint32_t target_page,
                                                     std::uint32_t target_block) const;

    // program_load 阶段标记潜在违规；真正拒绝通常延迟到 program_execute。
    virtual bool marks_nand_program_load_violation(const ModelConfig& config,
                                                   const RuntimeState& state,
                                                   std::uint16_t column,
                                                   std::uint64_t length) const;
};

// 根据配置开关创建能力模块链，builder 和 validator 都复用这条入口。
std::vector<std::unique_ptr<CapabilityModule>> create_capability_modules(const ModelConfig& config);

} // namespace flash_model

#endif
