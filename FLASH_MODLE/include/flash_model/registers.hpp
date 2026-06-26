#ifndef FLASH_MODEL_REGISTERS_HPP
#define FLASH_MODEL_REGISTERS_HPP

#include "flash_model/capability.hpp"
#include "flash_model/config.hpp"

#include <cstdint>

namespace flash_model {

// Register/status base shared by NOR and NAND flows.
class RegisterEngine {
public:
    explicit RegisterEngine(const ModelConfig& config);

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
