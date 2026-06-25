#include "flash_model/register_engine.hpp"

namespace flash_model {

RegisterEngine::RegisterEngine(const ModelConfig& config) : config_(config)
{
    load_defaults_preserving_capabilities();
}

void RegisterEngine::load_defaults_preserving_capabilities()
{
    const auto enabled_capabilities = state_.enabled_capabilities;

    state_ = RuntimeState{};
    state_.enabled_capabilities = enabled_capabilities;

    state_.status1 = config_.registers.status1_default;
    state_.status2 = config_.registers.status2_default;
    state_.status3 = config_.registers.status3_default;

    // A0h/B0h/C0h/D0h 是 SPI-NAND 常见 Feature Register 地址。
    // NOR 配置也可以保留这些默认值为 0；RegisterEngine 不在这里判断器件类型。
    state_.features[0xA0] = config_.registers.feature_a0_default;
    state_.features[0xB0] = config_.registers.feature_b0_default;
    state_.features[0xC0] = config_.registers.feature_c0_default;
    state_.features[0xD0] = config_.registers.feature_d0_default;

    // 当前统一约定：SR2 bit1 可作为通用 QE 默认值来源。
    // 如果某厂商 QE 位不同，应由 DevicePolicy::on_power_on() 进行修正。
    state_.quad_enabled = (state_.status2 & 0x02u) != 0;

    wel_ = false;
    program_fail_ = false;
    erase_fail_ = false;
}

void RegisterEngine::reset()
{
    load_defaults_preserving_capabilities();
}

void RegisterEngine::set_write_enable(bool value)
{
    wel_ = value;

    // SR1 bit1 是常见 NOR WEL 位；这里作为内部规范化显示。
    // SPI-NAND 的 WEL 会在读取 Feature C0h 时动态合成。
    if (wel_) state_.status1 |= 0x02u;
    else state_.status1 &= static_cast<std::uint8_t>(~0x02u);
}

void RegisterEngine::set_program_fail(bool value)
{
    program_fail_ = value;
}

void RegisterEngine::set_erase_fail(bool value)
{
    erase_fail_ = value;
}

void RegisterEngine::clear_fail_bits()
{
    program_fail_ = false;
    erase_fail_ = false;
}

std::uint8_t RegisterEngine::feature(std::uint8_t address) const
{
    const auto it = state_.features.find(address);
    return it == state_.features.end() ? 0 : it->second;
}

std::uint8_t RegisterEngine::get_feature(std::uint8_t address, bool busy) const
{
    if (address != 0xC0 || !config_.registers.feature_c0_dynamic) {
        return feature(address);
    }

    // SPI-NAND C0h status register 的低位在许多 datasheet 中表示：
    // bit0 OIP/BUSY, bit1 WEL, bit2 E_FAIL, bit3 P_FAIL。
    // 高四位保留配置默认值，便于后续 policy 注入 ECCS 等厂商语义。
    std::uint8_t value = feature(0xC0) & 0xF0u;
    if (busy) value |= 0x01u;
    if (wel_) value |= 0x02u;
    if (erase_fail_) value |= 0x04u;
    if (program_fail_) value |= 0x08u;
    return value;
}

void RegisterEngine::set_feature(std::uint8_t address, std::uint8_t value)
{
    state_.features[address] = value;
}

} // namespace flash_model
