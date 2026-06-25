#ifndef FLASH_MODEL_REGISTER_ENGINE_HPP
#define FLASH_MODEL_REGISTER_ENGINE_HPP

#include "flash_model/capability_module.hpp"
#include "flash_model/config.hpp"

#include <cstdint>

namespace flash_model {

// RegisterEngine 是 Common Core 里的“寄存器/状态基座”。
//
// 它只管理软件可观察状态，不直接执行读/写/擦命令：
// - WEL 由 write-enable/write-disable 或命令完成后设置。
// - P_FAIL/E_FAIL 由具体行为失败处设置。
// - SPI-NAND Feature C0h 是动态状态寄存器，读取时合成 BUSY/WEL/FAIL 位。
//
// 这样 FlashModel 可以专注命令流程，capability/policy 后续也可以围绕统一
// 寄存器状态扩展私有规则，而不是继续把状态位同步逻辑散落在各个命令函数里。
class RegisterEngine {
public:
    explicit RegisterEngine(const ModelConfig& config);

    // reset() 恢复寄存器默认值和失败状态，但保留 enabled_capabilities。
    // enabled_capabilities 描述模型组合结构，不是芯片内部可复位寄存器。
    void reset();

    RuntimeState& mutable_state() { return state_; }
    const RuntimeState& state() const { return state_; }

    bool wel() const { return wel_; }
    void set_write_enable(bool value);

    void set_program_fail(bool value);
    void set_erase_fail(bool value);
    void clear_fail_bits();

    std::uint8_t feature(std::uint8_t address) const;
    std::uint8_t get_feature(std::uint8_t address, bool busy) const;
    void set_feature(std::uint8_t address, std::uint8_t value);

private:
    ModelConfig config_;
    RuntimeState state_;
    bool wel_ = false;
    bool program_fail_ = false;
    bool erase_fail_ = false;

    void load_defaults_preserving_capabilities();
};

} // namespace flash_model

#endif
