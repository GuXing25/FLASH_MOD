#ifndef FLASH_MODEL_CAPABILITY_MODULE_HPP
#define FLASH_MODEL_CAPABILITY_MODULE_HPP

#include "flash_model/config.hpp"
#include "flash_model/validator.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace flash_model {

// RuntimeState 是“组合后的模型运行时状态”。
//
// 注意它不是完整器件状态的永久归宿：随着架构继续拆分，寄存器状态由
// RegisterEngine 负责，地址由 AddressMapper 负责，存储阵列由 StorageBackend 负责。
// RuntimeState 主要作为 capability/policy 的共享状态视图，让模块可以看到并调整
// 当前 status/feature/能力挂载结果。
struct RuntimeState {
    std::uint8_t status1 = 0;
    std::uint8_t status2 = 0;
    std::uint8_t status3 = 0;
    std::map<std::uint8_t, std::uint8_t> features;
    bool quad_enabled = false;
    std::vector<std::string> enabled_capabilities;
};

// CapabilityModule 表示“多颗芯片都会出现，但不是所有芯片都有”的功能块。
//
// 一个 module 应该回答三件事：
// 1. name(): 这个能力叫什么，便于日志、测试和能力矩阵记录。
// 2. validate(): 该能力启用时配置是否完整、是否与器件类型冲突。
// 3. attach(): 模型构建时把能力挂到 RuntimeState 上。
//
// 当前第一版 module 还比较薄，主要负责注册与校验；后续应逐步把 OTP、
// security register、copy-back 等真实行为从 FlashModel 中拆到对应 module。
class CapabilityModule {
public:
    virtual ~CapabilityModule() = default;

    virtual std::string name() const = 0;
    virtual void validate(const ModelConfig& config, ValidationReport& report) const = 0;
    virtual void attach(RuntimeState& state) const = 0;
};

std::vector<std::unique_ptr<CapabilityModule>> create_capability_modules(const ModelConfig& config);

} // namespace flash_model

#endif
