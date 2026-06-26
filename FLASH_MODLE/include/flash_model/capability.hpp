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

// Shared runtime view used by capability modules and policies.
struct RuntimeState {
    std::uint8_t status1 = 0;
    std::uint8_t status2 = 0;
    std::uint8_t status3 = 0;
    std::map<std::uint8_t, std::uint8_t> features;
    bool quad_enabled = false;
    bool deep_power_down = false;
    bool suspended = false;
    bool otp_mode = false;
    std::uint32_t address_bytes = 3;
    std::uint32_t active_die = 0;
    std::uint32_t active_plane = 0;
    std::uint32_t read_retry_level = 0;
    std::vector<std::string> enabled_capabilities;
    std::vector<std::uint32_t> bad_blocks;
};

struct CapabilityDecision {
    bool ok = true;
    std::string message;

    static CapabilityDecision allow() { return CapabilityDecision{}; }
    static CapabilityDecision reject(const std::string& reason)
    {
        CapabilityDecision decision;
        decision.ok = false;
        decision.message = reason;
        return decision;
    }
};

// Optional feature block. Hooks default to allow; modules override only what they own.
class CapabilityModule {
public:
    virtual ~CapabilityModule() = default;

    virtual std::string name() const = 0;
    virtual void validate(const ModelConfig& config, ValidationReport& report) const = 0;
    virtual void attach(RuntimeState& state) const = 0;

    virtual CapabilityDecision before_nor_program(const ModelConfig& config,
                                                  const RuntimeState& state,
                                                  std::uint64_t address,
                                                  std::uint64_t length) const;
    virtual CapabilityDecision before_nor_erase(const ModelConfig& config,
                                                const RuntimeState& state,
                                                std::uint64_t address,
                                                std::uint64_t length) const;
    virtual CapabilityDecision before_nand_block_erase(const ModelConfig& config,
                                                       const RuntimeState& state,
                                                       std::uint32_t block) const;
    virtual CapabilityDecision before_nand_program_execute(const ModelConfig& config,
                                                           const RuntimeState& state,
                                                           std::uint32_t page,
                                                           std::uint32_t block,
                                                           bool cached_program_violation) const;
    virtual CapabilityDecision before_nand_copy_back(const ModelConfig& config,
                                                     const RuntimeState& state,
                                                     std::uint32_t source_page,
                                                     std::uint32_t target_page,
                                                     std::uint32_t target_block) const;

    virtual bool marks_nand_program_load_violation(const ModelConfig& config,
                                                   const RuntimeState& state,
                                                   std::uint16_t column,
                                                   std::uint64_t length) const;
};

std::vector<std::unique_ptr<CapabilityModule>> create_capability_modules(const ModelConfig& config);

} // namespace flash_model

#endif
