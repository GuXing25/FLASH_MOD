#ifndef FLASH_MODEL_REGISTERS_HPP
#define FLASH_MODEL_REGISTERS_HPP

#include "flash_model/capability.hpp"
#include "flash_model/config.hpp"

#include <cstdint>

namespace flash_model {

// RegisterEngine 管理 NOR/SPI-NAND 共享的状态寄存器和运行态标志。
class RegisterEngine {
public:
    explicit RegisterEngine(const ModelConfig& config);

    // reset 恢复寄存器默认值，但保留 capability attach 后记录的能力列表。
    void reset();

    RuntimeState& mutable_state() { return state_; }
    const RuntimeState& state() const { return state_; }

    // WEL 是写/擦命令的公共门禁。
    bool wel() const { return wel_; }
    void set_write_enable(bool value);

    // program/erase fail 位由模型或 capability hook 设置，再由 feature/status 读出。
    void set_program_fail(bool value);
    void set_erase_fail(bool value);
    void clear_fail_bits();

    // feature 接口主要给 SPI-NAND 使用；status 也可通过 feature_c0_dynamic 合成。
    std::uint8_t feature(std::uint8_t address) const;
    std::uint8_t get_feature(std::uint8_t address, bool busy) const;
    void set_feature(std::uint8_t address, std::uint8_t value);

private:
    ModelConfig config_; // 寄存器默认值来自配置。
    RuntimeState state_; // 当前运行态。
    bool wel_ = false; // Write Enable Latch。
    bool program_fail_ = false; // program fail 合成位。
    bool erase_fail_ = false; // erase fail 合成位。

    void load_defaults_preserving_capabilities();
};

} // namespace flash_model

#endif
