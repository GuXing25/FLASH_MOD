#include "flash_model/registers.hpp"

namespace flash_model {

RegisterEngine::RegisterEngine(const ModelConfig& config) : config_(config)
{
    load_defaults_preserving_capabilities();
}

void RegisterEngine::load_defaults_preserving_capabilities()
{
    const auto enabled_capabilities = state_.enabled_capabilities;
    const auto bad_blocks = state_.bad_blocks;

    state_ = RuntimeState{};
    state_.enabled_capabilities = enabled_capabilities;
    state_.bad_blocks = bad_blocks;

    state_.status1 = config_.registers.status1_default;
    state_.status2 = config_.registers.status2_default;
    state_.status3 = config_.registers.status3_default;

    state_.features[0xA0] = config_.registers.feature_a0_default;
    state_.features[0xB0] = config_.registers.feature_b0_default;
    state_.features[0xC0] = config_.registers.feature_c0_default;
    state_.features[0xD0] = config_.registers.feature_d0_default;

    state_.quad_enabled = (state_.status2 & 0x02u) != 0;
    state_.address_bytes = config_.constraints.address_bytes == 0
                                ? 3
                                : config_.constraints.address_bytes;
    state_.active_die = 0;
    state_.active_plane = 0;
    state_.read_retry_level = 0;

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
