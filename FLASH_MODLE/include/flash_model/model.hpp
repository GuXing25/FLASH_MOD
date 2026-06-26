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

struct OperationResult {
    bool ok = true;
    std::string message;
    std::vector<std::uint8_t> data;
    double now_us = 0.0;
};

// Thin command facade that wires core, capabilities, and policy together.
class FlashModel {
public:
    FlashModel(ModelConfig config,
               std::unique_ptr<DevicePolicy> policy,
               std::vector<std::unique_ptr<CapabilityModule>> modules);

    FlashModel(FlashModel&&) noexcept = default;
    FlashModel& operator=(FlashModel&&) noexcept = default;
    FlashModel(const FlashModel&) = delete;
    FlashModel& operator=(const FlashModel&) = delete;

    const ModelConfig& config() const { return config_; }
    const RuntimeState& state() const { return registers_.state(); }
    double time_us() const { return timing_.now_us(); }
    bool busy();

    OperationResult read_id();
    OperationResult write_enable();
    OperationResult write_disable();
    OperationResult wait_ready();
    OperationResult reset();
    OperationResult deep_power_down();
    OperationResult release_power_down();
    OperationResult enter_4byte_address();
    OperationResult exit_4byte_address();
    OperationResult suspend();
    OperationResult resume();
    OperationResult read_unique_id();
    OperationResult read_sfdp(std::uint32_t offset, std::size_t length);

    OperationResult nor_read(std::uint64_t address, std::size_t length);
    OperationResult nor_program(std::uint64_t address, const std::vector<std::uint8_t>& data);
    OperationResult nor_sector_erase(std::uint64_t address);
    OperationResult nor_block32_erase(std::uint64_t address);
    OperationResult nor_block_erase(std::uint64_t address);
    OperationResult nor_chip_erase();
    OperationResult read_security_register(std::uint8_t index,
                                           std::uint16_t offset,
                                           std::size_t length);
    OperationResult erase_security_register(std::uint8_t index);
    OperationResult program_security_register(std::uint8_t index,
                                              std::uint16_t offset,
                                              const std::vector<std::uint8_t>& data);
    OperationResult lock_security_register(std::uint8_t index);

    OperationResult enter_otp_mode();
    OperationResult exit_otp_mode();
    OperationResult get_feature(std::uint8_t address);
    OperationResult set_feature(std::uint8_t address, std::uint8_t value);
    OperationResult select_die(std::uint32_t die);
    OperationResult select_plane(std::uint32_t plane);
    OperationResult set_read_retry_level(std::uint32_t level);
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
    ModelConfig config_;
    std::unique_ptr<DevicePolicy> policy_;
    std::vector<std::unique_ptr<CapabilityModule>> modules_;
    RegisterEngine registers_;
    AddressMapper address_;
    TimingEngine timing_;
    InterfaceEngine interface_;
    SparseStorageBackend storage_;

    std::vector<std::uint8_t> cache_;
    bool cache_valid_ = false;
    bool cache_program_violation_ = false;
    std::map<std::uint32_t, std::uint32_t> partial_program_count_;
    std::map<std::uint32_t, std::uint32_t> otp_program_count_;
    std::map<std::uint32_t, std::uint32_t> next_page_by_block_;
    std::vector<std::uint8_t> security_;
    std::vector<bool> security_locked_;
    SparseStorageBackend otp_storage_;
    std::vector<std::uint8_t> sfdp_;
    std::vector<std::uint8_t> parameter_page_;

    OperationResult result(bool ok, const std::string& message,
                           const std::vector<std::uint8_t>& data = {});
    bool require_awake(const char* op) const;
    bool require_ready(const char* op);
    bool require_wel(const char* op);
    bool require_not_suspended(const char* op) const;
    CapabilityDecision before_nor_program(std::uint64_t address, std::uint64_t length) const;
    CapabilityDecision before_nor_erase(std::uint64_t address, std::uint64_t length) const;
    CapabilityDecision before_nand_block_erase(std::uint32_t block) const;
    CapabilityDecision before_nand_program_execute(std::uint32_t page, std::uint32_t block) const;
    CapabilityDecision before_nand_copy_back(std::uint32_t source_page,
                                             std::uint32_t target_page,
                                             std::uint32_t target_block) const;
    bool marks_nand_program_load_violation(std::uint16_t column, std::uint64_t length) const;
    bool security_range_ok(std::uint8_t index, std::uint16_t offset, std::size_t length) const;
    std::uint64_t security_offset(std::uint8_t index, std::uint16_t offset) const;
    std::uint32_t security_register_count() const;
    std::uint32_t security_register_size() const;
    std::vector<std::uint8_t> generated_unique_id() const;
    void build_parameter_page();
    bool otp_page_ok(std::uint32_t page) const;
    std::uint32_t command_address_bytes() const;
    void account_interface(std::uint64_t request_bytes,
                           std::uint64_t response_bytes,
                           bool turnaround = true);
    void account_command(const std::string& command_name,
                         std::uint64_t write_payload_bytes = 0,
                         std::uint64_t read_response_bytes = 0);
    OperationResult execute_cached_program(std::uint32_t page,
                                           const std::string& op,
                                           const std::string& accepted_message);
};

} // namespace flash_model

#endif
