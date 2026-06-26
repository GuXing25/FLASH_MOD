#include "flash_model/model.hpp"

#include "flash_model/command.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace flash_model {
namespace {

std::string hex_byte(std::uint8_t value)
{
    const char* digits = "0123456789ABCDEF";
    std::string out = "0x";
    out.push_back(digits[(value >> 4) & 0x0F]);
    out.push_back(digits[value & 0x0F]);
    return out;
}

} // namespace

FlashModel::FlashModel(ModelConfig config,
                       std::unique_ptr<DevicePolicy> policy,
                       std::vector<std::unique_ptr<CapabilityModule>> modules)
    : config_(std::move(config)),
      policy_(std::move(policy)),
      modules_(std::move(modules)),
      registers_(config_),
      address_(config_),
      timing_(),
      interface_(config_),
      storage_(config_.total_size_bytes())
{
    for (const auto& module : modules_) module->attach(registers_.mutable_state());
    if (policy_) policy_->on_power_on(registers_.mutable_state());
    if (config_.effective_page_size() != 0) {
        cache_.assign(config_.effective_page_size(), 0xFF);
    }
    if (config_.capabilities.security_register) {
        const std::uint64_t bytes =
            static_cast<std::uint64_t>(security_register_count()) *
            static_cast<std::uint64_t>(security_register_size());
        security_.assign(static_cast<std::size_t>(bytes), 0xFF);
        security_locked_.assign(security_register_count(), false);
    }
    if (config_.capabilities.otp && config_.constraints.otp_page_count != 0) {
        const std::uint64_t bytes =
            static_cast<std::uint64_t>(config_.constraints.otp_page_count) *
            static_cast<std::uint64_t>(config_.effective_page_size());
        otp_storage_.resize(bytes);
    }
    if (config_.capabilities.sfdp && config_.constraints.sfdp_size != 0) {
        sfdp_.assign(config_.constraints.sfdp_size, 0xFF);
        if (sfdp_.size() >= 8) {
            sfdp_[0] = 'S';
            sfdp_[1] = 'F';
            sfdp_[2] = 'D';
            sfdp_[3] = 'P';
            sfdp_[4] = 0x06;
            sfdp_[5] = 0x01;
            sfdp_[6] = 0x00;
            sfdp_[7] = 0xFF;
        }
        if (sfdp_.size() > 16 && !config_.device.id.empty()) {
            sfdp_[16] = config_.device.id.front();
        }
    }
    if (config_.capabilities.parameter_page && config_.constraints.parameter_page_size != 0) {
        build_parameter_page();
    }
}

OperationResult FlashModel::result(bool ok,
                                   const std::string& message,
                                   const std::vector<std::uint8_t>& data)
{
    return OperationResult{ok, message, data, timing_.now_us()};
}

std::uint32_t FlashModel::command_address_bytes() const
{
    return registers_.state().address_bytes == 0
               ? config_.constraints.address_bytes
               : registers_.state().address_bytes;
}

void FlashModel::account_interface(std::uint64_t request_bytes,
                                   std::uint64_t response_bytes,
                                   bool turnaround)
{
    timing_.advance(interface_.transfer_time_us(request_bytes, response_bytes, turnaround));
}

void FlashModel::account_command(const std::string& command_name,
                                 std::uint64_t write_payload_bytes,
                                 std::uint64_t read_response_bytes)
{
    const CommandTransfer transfer =
        describe_transfer(config_,
                          command_name,
                          command_address_bytes(),
                          write_payload_bytes,
                          read_response_bytes);
    account_interface(transfer.request_bytes, transfer.response_bytes, transfer.turnaround);
}

bool FlashModel::busy()
{
    return timing_.busy();
}

bool FlashModel::require_awake(const char* op) const
{
    (void)op;
    return !registers_.state().deep_power_down;
}

bool FlashModel::require_ready(const char* op)
{
    if (!timing_.busy()) return true;
    (void)op;
    return false;
}

bool FlashModel::require_wel(const char* op)
{
    if (!config_.constraints.require_wel || registers_.wel()) return true;
    (void)op;
    return false;
}

bool FlashModel::require_not_suspended(const char* op) const
{
    // A suspended program/erase window may allow reads, but this model rejects
    // new mutating commands until RESUME finishes the outstanding operation.
    if (!registers_.state().suspended) return true;
    (void)op;
    return false;
}

CapabilityDecision FlashModel::before_nor_program(std::uint64_t address, std::uint64_t length) const
{
    for (const auto& module : modules_) {
        CapabilityDecision decision =
            module->before_nor_program(config_, registers_.state(), address, length);
        if (!decision.ok) return decision;
    }
    return CapabilityDecision::allow();
}

CapabilityDecision FlashModel::before_nor_erase(std::uint64_t address, std::uint64_t length) const
{
    for (const auto& module : modules_) {
        CapabilityDecision decision =
            module->before_nor_erase(config_, registers_.state(), address, length);
        if (!decision.ok) return decision;
    }
    return CapabilityDecision::allow();
}

CapabilityDecision FlashModel::before_nand_block_erase(std::uint32_t block) const
{
    for (const auto& module : modules_) {
        CapabilityDecision decision =
            module->before_nand_block_erase(config_, registers_.state(), block);
        if (!decision.ok) return decision;
    }
    return CapabilityDecision::allow();
}

CapabilityDecision FlashModel::before_nand_program_execute(std::uint32_t page, std::uint32_t block) const
{
    for (const auto& module : modules_) {
        CapabilityDecision decision =
            module->before_nand_program_execute(config_, registers_.state(), page, block,
                                                cache_program_violation_);
        if (!decision.ok) return decision;
    }
    return CapabilityDecision::allow();
}

CapabilityDecision FlashModel::before_nand_copy_back(std::uint32_t source_page,
                                                     std::uint32_t target_page,
                                                     std::uint32_t target_block) const
{
    for (const auto& module : modules_) {
        CapabilityDecision decision =
            module->before_nand_copy_back(config_, registers_.state(), source_page, target_page,
                                          target_block);
        if (!decision.ok) return decision;
    }
    return CapabilityDecision::allow();
}

bool FlashModel::marks_nand_program_load_violation(std::uint16_t column, std::uint64_t length) const
{
    for (const auto& module : modules_) {
        if (module->marks_nand_program_load_violation(config_, registers_.state(), column, length)) {
            return true;
        }
    }
    return false;
}

OperationResult FlashModel::read_id()
{
    if (!config_.commands.read_id) return result(false, "READ_ID unsupported");
    if (!require_awake("READ_ID")) return result(false, "READ_ID rejected: deep power-down");
    if (!require_ready("READ_ID")) return result(false, "READ_ID rejected: busy");
    account_command("read_id", 0, config_.device.id.size());
    timing_.advance(0.1);
    return result(true, "READ_ID", config_.device.id);
}

OperationResult FlashModel::write_enable()
{
    if (!require_awake("WRITE_ENABLE")) return result(false, "WRITE_ENABLE rejected: deep power-down");
    if (!require_ready("WRITE_ENABLE")) return result(false, "WRITE_ENABLE rejected: busy");
    account_command("write_enable");
    registers_.set_write_enable(true);
    timing_.advance(0.1);
    return result(true, "WRITE_ENABLE: WEL=1");
}

OperationResult FlashModel::write_disable()
{
    if (!require_awake("WRITE_DISABLE")) return result(false, "WRITE_DISABLE rejected: deep power-down");
    account_command("write_disable");
    registers_.set_write_enable(false);
    timing_.advance(0.1);
    return result(true, "WRITE_DISABLE: WEL=0");
}

OperationResult FlashModel::wait_ready()
{
    timing_.wait_ready();
    return result(true, "WAIT_READY");
}

OperationResult FlashModel::reset()
{
    if (!config_.commands.reset) return result(false, "RESET unsupported");
    timing_.clear_busy();
    account_command("reset");
    registers_.reset();
    if (policy_) policy_->on_power_on(registers_.mutable_state());
    timing_.advance(config_.timing.reset_us);
    return result(true, "RESET");
}

OperationResult FlashModel::deep_power_down()
{
    if (is_nand_like(config_.device.cls)) return result(false, "DEEP_POWER_DOWN rejected: NAND-like device");
    if (!config_.commands.deep_power_down) return result(false, "DEEP_POWER_DOWN unsupported");
    if (!config_.capabilities.deep_power_down) return result(false, "DEEP_POWER_DOWN rejected: capability disabled");
    if (!require_ready("DEEP_POWER_DOWN")) return result(false, "DEEP_POWER_DOWN rejected: busy");
    account_command("deep_power_down");
    registers_.set_write_enable(false);
    registers_.mutable_state().deep_power_down = true;
    timing_.advance(0.1);
    return result(true, "DEEP_POWER_DOWN");
}

OperationResult FlashModel::release_power_down()
{
    if (is_nand_like(config_.device.cls)) return result(false, "RELEASE_POWER_DOWN rejected: NAND-like device");
    if (!config_.commands.release_power_down) return result(false, "RELEASE_POWER_DOWN unsupported");
    if (!config_.capabilities.deep_power_down) return result(false, "RELEASE_POWER_DOWN rejected: capability disabled");
    account_command("release_power_down");
    registers_.mutable_state().deep_power_down = false;
    timing_.advance(0.1);
    return result(true, "RELEASE_POWER_DOWN");
}

OperationResult FlashModel::enter_4byte_address()
{
    if (is_nand_like(config_.device.cls)) return result(false, "ENTER_4BYTE rejected: NAND-like device");
    if (!config_.commands.enter_4byte_address) return result(false, "ENTER_4BYTE unsupported");
    if (!config_.capabilities.four_byte_address) return result(false, "ENTER_4BYTE rejected: capability disabled");
    if (!require_awake("ENTER_4BYTE")) return result(false, "ENTER_4BYTE rejected: deep power-down");
    if (!require_ready("ENTER_4BYTE")) return result(false, "ENTER_4BYTE rejected: busy");
    account_command("enter_4byte_address");
    registers_.mutable_state().address_bytes = 4;
    timing_.advance(0.1);
    return result(true, "ENTER_4BYTE");
}

OperationResult FlashModel::exit_4byte_address()
{
    if (is_nand_like(config_.device.cls)) return result(false, "EXIT_4BYTE rejected: NAND-like device");
    if (!config_.commands.exit_4byte_address) return result(false, "EXIT_4BYTE unsupported");
    if (!config_.capabilities.four_byte_address) return result(false, "EXIT_4BYTE rejected: capability disabled");
    if (!require_awake("EXIT_4BYTE")) return result(false, "EXIT_4BYTE rejected: deep power-down");
    if (!require_ready("EXIT_4BYTE")) return result(false, "EXIT_4BYTE rejected: busy");
    account_command("exit_4byte_address");
    registers_.mutable_state().address_bytes = 3;
    timing_.advance(0.1);
    return result(true, "EXIT_4BYTE");
}

OperationResult FlashModel::suspend()
{
    if (!config_.commands.suspend) return result(false, "SUSPEND unsupported");
    if (!config_.capabilities.suspend_resume) return result(false, "SUSPEND rejected: capability disabled");
    if (registers_.state().suspended) return result(false, "SUSPEND rejected: already suspended");
    account_command("suspend");
    if (!timing_.suspend()) return result(false, "SUSPEND rejected: no busy operation");
    registers_.mutable_state().suspended = true;
    timing_.advance(0.1);
    return result(true, "SUSPEND");
}

OperationResult FlashModel::resume()
{
    if (!config_.commands.resume) return result(false, "RESUME unsupported");
    if (!config_.capabilities.suspend_resume) return result(false, "RESUME rejected: capability disabled");
    if (!registers_.state().suspended) return result(false, "RESUME rejected: not suspended");
    account_command("resume");
    if (!timing_.resume()) return result(false, "RESUME rejected: timing state");
    registers_.mutable_state().suspended = false;
    timing_.advance(0.1);
    return result(true, "RESUME");
}

std::vector<std::uint8_t> FlashModel::generated_unique_id() const
{
    if (!config_.device.unique_id.empty()) return config_.device.unique_id;

    const std::size_t size = config_.constraints.unique_id_size == 0
                                 ? 8
                                 : config_.constraints.unique_id_size;
    std::vector<std::uint8_t> out(size, 0xFF);
    std::uint32_t seed = 0xA55A5AA5u;
    for (std::uint8_t byte : config_.device.id) {
        seed = (seed * 131u) ^ byte;
    }
    for (char c : config_.device.name) {
        seed = (seed * 131u) ^ static_cast<unsigned char>(c);
    }
    for (std::size_t i = 0; i < out.size(); ++i) {
        seed = seed * 1664525u + 1013904223u;
        out[i] = static_cast<std::uint8_t>((seed >> 24) & 0xFFu);
    }
    return out;
}

OperationResult FlashModel::read_unique_id()
{
    if (!config_.commands.read_unique_id) return result(false, "READ_UNIQUE_ID unsupported");
    if (!config_.capabilities.unique_id) return result(false, "READ_UNIQUE_ID rejected: capability disabled");
    if (!require_awake("READ_UNIQUE_ID")) return result(false, "READ_UNIQUE_ID rejected: deep power-down");
    if (!require_ready("READ_UNIQUE_ID")) return result(false, "READ_UNIQUE_ID rejected: busy");
    std::vector<std::uint8_t> unique_id = generated_unique_id();
    account_command("read_unique_id", 0, unique_id.size());
    timing_.advance(config_.timing.read_us);
    return result(true, "READ_UNIQUE_ID", unique_id);
}

void FlashModel::build_parameter_page()
{
    const std::size_t size = config_.constraints.parameter_page_size == 0
                                 ? 256
                                 : config_.constraints.parameter_page_size;
    parameter_page_.assign(size, 0x00);
    if (parameter_page_.size() < 4) return;

    parameter_page_[0] = 'O';
    parameter_page_[1] = 'N';
    parameter_page_[2] = 'F';
    parameter_page_[3] = 'I';

    auto put_ascii = [this](std::size_t offset, std::size_t width, const std::string& text) {
        if (offset >= parameter_page_.size()) return;
        const std::size_t end = std::min(parameter_page_.size(), offset + width);
        std::fill(parameter_page_.begin() + offset, parameter_page_.begin() + end, 0x20);
        const std::size_t n = std::min<std::size_t>(text.size(), end - offset);
        std::copy(text.begin(), text.begin() + n, parameter_page_.begin() + offset);
    };
    auto put_u16 = [this](std::size_t offset, std::uint32_t value) {
        if (offset + 1 >= parameter_page_.size()) return;
        parameter_page_[offset] = static_cast<std::uint8_t>(value & 0xFFu);
        parameter_page_[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    };
    auto put_u8 = [this](std::size_t offset, std::uint32_t value) {
        if (offset >= parameter_page_.size()) return;
        parameter_page_[offset] = static_cast<std::uint8_t>(value & 0xFFu);
    };
    auto put_u32 = [this](std::size_t offset, std::uint32_t value) {
        if (offset + 3 >= parameter_page_.size()) return;
        parameter_page_[offset] = static_cast<std::uint8_t>(value & 0xFFu);
        parameter_page_[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
        parameter_page_[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
        parameter_page_[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
    };
    auto microseconds_u16 = [](double value) {
        if (value <= 0.0) return std::uint32_t{0};
        if (value > 65535.0) return std::uint32_t{65535};
        return static_cast<std::uint32_t>(value + 0.5);
    };

    put_ascii(32, 12, config_.device.manufacturer);
    put_ascii(44, 20, config_.device.name);
    if (parameter_page_.size() > 64 && !config_.device.id.empty()) {
        parameter_page_[64] = config_.device.id.front();
    }
    put_u32(80, config_.geometry.main_size);
    put_u16(84, config_.geometry.spare_size);
    put_u32(92, config_.geometry.pages_per_block);
    put_u32(96, config_.geometry.blocks);
    put_u8(100, config_.geometry.dies == 0 ? 1 : config_.geometry.dies);
    put_u8(102, 1);
    if (config_.constraints.minimum_valid_blocks != 0 &&
        config_.constraints.minimum_valid_blocks <= config_.geometry.blocks) {
        put_u16(103, config_.geometry.blocks - config_.constraints.minimum_valid_blocks);
    }
    put_u8(110, config_.constraints.max_partial_programs);
    put_u8(112, config_.constraints.ecc_bits);
    put_u16(133, microseconds_u16(config_.timing.program_us));
    put_u16(135, microseconds_u16(config_.timing.block_erase_us));
    put_u16(137, microseconds_u16(config_.timing.page_read_us));
    put_u8(248, config_.constraints.ecc_bits);

    // NAND parameter pages commonly expose repeated 256-byte copies so hosts can
    // recover the descriptor even if one copy is corrupted.
    if (parameter_page_.size() > 256) {
        const std::size_t primary = 256;
        for (std::size_t offset = primary; offset < parameter_page_.size(); offset += primary) {
            const std::size_t n = std::min(primary, parameter_page_.size() - offset);
            std::copy(parameter_page_.begin(), parameter_page_.begin() + n,
                      parameter_page_.begin() + offset);
        }
    }
}

OperationResult FlashModel::read_sfdp(std::uint32_t offset, std::size_t length)
{
    if (is_nand_like(config_.device.cls)) return result(false, "READ_SFDP rejected: NAND-like device");
    if (!config_.commands.read_sfdp) return result(false, "READ_SFDP unsupported");
    if (!config_.capabilities.sfdp) return result(false, "READ_SFDP rejected: capability disabled");
    if (!require_awake("READ_SFDP")) return result(false, "READ_SFDP rejected: deep power-down");
    if (!require_ready("READ_SFDP")) return result(false, "READ_SFDP rejected: busy");
    if (offset > sfdp_.size() || length > sfdp_.size() - offset) {
        return result(false, "READ_SFDP rejected: range");
    }
    std::vector<std::uint8_t> out(sfdp_.begin() + offset, sfdp_.begin() + offset + length);
    account_command("read_sfdp", 0, out.size());
    timing_.advance(config_.timing.read_us);
    return result(true, "READ_SFDP", out);
}

OperationResult FlashModel::nor_read(std::uint64_t address, std::size_t length)
{
    if (is_nand_like(config_.device.cls)) return result(false, "NOR_READ rejected: NAND-like device");
    if (!config_.commands.nor_read) return result(false, "NOR_READ unsupported");
    if (!require_awake("NOR_READ")) return result(false, "NOR_READ rejected: deep power-down");
    if (!require_ready("NOR_READ")) return result(false, "NOR_READ rejected: busy");
    account_command("nor_read", 0, length);
    timing_.advance(config_.timing.read_us);
    return result(true, "NOR_READ", storage_.read(address, length));
}

OperationResult FlashModel::nor_program(std::uint64_t address, const std::vector<std::uint8_t>& data)
{
    if (is_nand_like(config_.device.cls)) return result(false, "NOR_PROGRAM rejected: NAND-like device");
    if (!config_.commands.nor_program) return result(false, "NOR_PROGRAM unsupported");
    if (!require_awake("NOR_PROGRAM")) return result(false, "NOR_PROGRAM rejected: deep power-down");
    if (!require_ready("NOR_PROGRAM")) return result(false, "NOR_PROGRAM rejected: busy");
    if (!require_not_suspended("NOR_PROGRAM")) return result(false, "NOR_PROGRAM rejected: suspended");
    if (!require_wel("NOR_PROGRAM")) return result(false, "NOR_PROGRAM rejected: WEL=0");
    const std::uint64_t page = config_.geometry.page_size;
    const std::uint64_t page_base = page == 0 ? address : (address / page) * page;
    const std::uint64_t page_offset = page == 0 ? 0 : address - page_base;
    const bool wraps = page != 0 && data.size() > page - page_offset;
    const CapabilityDecision capability =
        wraps ? before_nor_program(page_base, page) : before_nor_program(address, data.size());
    if (!capability.ok) {
        registers_.set_write_enable(false);
        registers_.set_program_fail(true);
        return result(false, capability.message);
    }
    account_command("nor_program", data.size());
    if (page == 0 || !wraps) {
        storage_.program_and(address, data);
    } else {
        for (std::size_t i = 0; i < data.size(); ++i) {
            const std::uint64_t target = page_base + ((page_offset + i) % page);
            storage_.program_and(target, {data[i]});
        }
    }
    registers_.set_write_enable(false);
    registers_.set_program_fail(false);
    timing_.start_busy(config_.timing.program_us);
    return result(true, "NOR_PROGRAM accepted");
}

OperationResult FlashModel::nor_sector_erase(std::uint64_t address)
{
    if (is_nand_like(config_.device.cls)) return result(false, "NOR_ERASE rejected: NAND-like device");
    if (!config_.commands.nor_erase) return result(false, "NOR_ERASE unsupported");
    if (!require_awake("NOR_ERASE")) return result(false, "NOR_ERASE rejected: deep power-down");
    if (!require_ready("NOR_ERASE")) return result(false, "NOR_ERASE rejected: busy");
    if (!require_not_suspended("NOR_ERASE")) return result(false, "NOR_ERASE rejected: suspended");
    if (!require_wel("NOR_ERASE")) return result(false, "NOR_ERASE rejected: WEL=0");
    const std::uint64_t sector = config_.geometry.sector_size;
    const std::uint64_t base = address_.nor_sector_base(address);
    const CapabilityDecision capability = before_nor_erase(base, sector);
    if (!capability.ok) {
        registers_.set_write_enable(false);
        registers_.set_erase_fail(true);
        return result(false, capability.message);
    }
    account_command("nor_sector_erase");
    storage_.erase(base, sector);
    registers_.set_write_enable(false);
    registers_.set_erase_fail(false);
    timing_.start_busy(config_.timing.sector_erase_us);
    return result(true, "NOR_SECTOR_ERASE accepted");
}

OperationResult FlashModel::nor_block32_erase(std::uint64_t address)
{
    if (is_nand_like(config_.device.cls)) return result(false, "NOR_BLOCK32_ERASE rejected: NAND-like device");
    if (!config_.commands.nor_block32_erase) return result(false, "NOR_BLOCK32_ERASE unsupported");
    if (!require_awake("NOR_BLOCK32_ERASE")) return result(false, "NOR_BLOCK32_ERASE rejected: deep power-down");
    if (!require_ready("NOR_BLOCK32_ERASE")) return result(false, "NOR_BLOCK32_ERASE rejected: busy");
    if (!require_not_suspended("NOR_BLOCK32_ERASE")) return result(false, "NOR_BLOCK32_ERASE rejected: suspended");
    if (!require_wel("NOR_BLOCK32_ERASE")) return result(false, "NOR_BLOCK32_ERASE rejected: WEL=0");
    const std::uint64_t block = config_.geometry.block32_size;
    if (block == 0) return result(false, "NOR_BLOCK32_ERASE rejected: block32_size=0");
    const std::uint64_t base = address_.nor_block32_base(address);
    const CapabilityDecision capability = before_nor_erase(base, block);
    if (!capability.ok) {
        registers_.set_write_enable(false);
        registers_.set_erase_fail(true);
        return result(false, capability.message);
    }
    account_command("nor_block32_erase");
    storage_.erase(base, block);
    registers_.set_write_enable(false);
    registers_.set_erase_fail(false);
    const double erase_us = config_.timing.block32_erase_us > 0.0
                                ? config_.timing.block32_erase_us
                                : config_.timing.block_erase_us;
    timing_.start_busy(erase_us);
    return result(true, "NOR_BLOCK32_ERASE accepted");
}

OperationResult FlashModel::nor_block_erase(std::uint64_t address)
{
    if (is_nand_like(config_.device.cls)) return result(false, "NOR_BLOCK_ERASE rejected: NAND-like device");
    if (!config_.commands.nor_block_erase) return result(false, "NOR_BLOCK_ERASE unsupported");
    if (!require_awake("NOR_BLOCK_ERASE")) return result(false, "NOR_BLOCK_ERASE rejected: deep power-down");
    if (!require_ready("NOR_BLOCK_ERASE")) return result(false, "NOR_BLOCK_ERASE rejected: busy");
    if (!require_not_suspended("NOR_BLOCK_ERASE")) return result(false, "NOR_BLOCK_ERASE rejected: suspended");
    if (!require_wel("NOR_BLOCK_ERASE")) return result(false, "NOR_BLOCK_ERASE rejected: WEL=0");
    const std::uint64_t block = config_.geometry.block_size;
    if (block == 0) return result(false, "NOR_BLOCK_ERASE rejected: block_size=0");
    const std::uint64_t base = address_.nor_block_base(address);
    const CapabilityDecision capability = before_nor_erase(base, block);
    if (!capability.ok) {
        registers_.set_write_enable(false);
        registers_.set_erase_fail(true);
        return result(false, capability.message);
    }
    account_command("nor_block_erase");
    storage_.erase(base, block);
    registers_.set_write_enable(false);
    registers_.set_erase_fail(false);
    timing_.start_busy(config_.timing.block_erase_us);
    return result(true, "NOR_BLOCK_ERASE accepted");
}

OperationResult FlashModel::nor_chip_erase()
{
    if (is_nand_like(config_.device.cls)) return result(false, "NOR_CHIP_ERASE rejected: NAND-like device");
    if (!config_.commands.nor_chip_erase) return result(false, "NOR_CHIP_ERASE unsupported");
    if (!require_awake("NOR_CHIP_ERASE")) return result(false, "NOR_CHIP_ERASE rejected: deep power-down");
    if (!require_ready("NOR_CHIP_ERASE")) return result(false, "NOR_CHIP_ERASE rejected: busy");
    if (!require_not_suspended("NOR_CHIP_ERASE")) return result(false, "NOR_CHIP_ERASE rejected: suspended");
    if (!require_wel("NOR_CHIP_ERASE")) return result(false, "NOR_CHIP_ERASE rejected: WEL=0");
    const CapabilityDecision capability = before_nor_erase(0, config_.geometry.memory_size);
    if (!capability.ok) {
        registers_.set_write_enable(false);
        registers_.set_erase_fail(true);
        return result(false, capability.message);
    }
    account_command("nor_chip_erase");
    storage_.erase(0, config_.geometry.memory_size);
    registers_.set_write_enable(false);
    registers_.set_erase_fail(false);
    timing_.start_busy(config_.timing.chip_erase_us);
    return result(true, "NOR_CHIP_ERASE accepted");
}

bool FlashModel::security_range_ok(std::uint8_t index,
                                   std::uint16_t offset,
                                   std::size_t length) const
{
    if (index >= security_register_count()) return false;
    if (offset > security_register_size()) return false;
    return length <= static_cast<std::size_t>(security_register_size() - offset);
}

std::uint64_t FlashModel::security_offset(std::uint8_t index, std::uint16_t offset) const
{
    return static_cast<std::uint64_t>(index) * security_register_size() + offset;
}

std::uint32_t FlashModel::security_register_count() const
{
    return config_.constraints.security_register_count == 0
               ? 3
               : config_.constraints.security_register_count;
}

std::uint32_t FlashModel::security_register_size() const
{
    return config_.constraints.security_register_size == 0
               ? 256
               : config_.constraints.security_register_size;
}

OperationResult FlashModel::read_security_register(std::uint8_t index,
                                                   std::uint16_t offset,
                                                   std::size_t length)
{
    if (is_nand_like(config_.device.cls)) return result(false, "SECURITY_READ rejected: NAND-like device");
    if (!config_.commands.read_security_register) return result(false, "SECURITY_READ unsupported");
    if (!config_.capabilities.security_register) return result(false, "SECURITY_READ rejected: capability disabled");
    if (!require_awake("SECURITY_READ")) return result(false, "SECURITY_READ rejected: deep power-down");
    if (!require_ready("SECURITY_READ")) return result(false, "SECURITY_READ rejected: busy");
    if (!security_range_ok(index, offset, length)) return result(false, "SECURITY_READ rejected: range");
    const std::uint64_t start = security_offset(index, offset);
    std::vector<std::uint8_t> out(security_.begin() + start,
                                  security_.begin() + start + length);
    account_command("security_read", 0, out.size());
    timing_.advance(config_.timing.read_us);
    return result(true, "SECURITY_READ", out);
}

OperationResult FlashModel::erase_security_register(std::uint8_t index)
{
    if (is_nand_like(config_.device.cls)) return result(false, "SECURITY_ERASE rejected: NAND-like device");
    if (!config_.commands.erase_security_register) return result(false, "SECURITY_ERASE unsupported");
    if (!config_.capabilities.security_register) return result(false, "SECURITY_ERASE rejected: capability disabled");
    if (!require_awake("SECURITY_ERASE")) return result(false, "SECURITY_ERASE rejected: deep power-down");
    if (!require_ready("SECURITY_ERASE")) return result(false, "SECURITY_ERASE rejected: busy");
    if (!require_not_suspended("SECURITY_ERASE")) return result(false, "SECURITY_ERASE rejected: suspended");
    if (!require_wel("SECURITY_ERASE")) return result(false, "SECURITY_ERASE rejected: WEL=0");
    if (!security_range_ok(index, 0, security_register_size())) return result(false, "SECURITY_ERASE rejected: range");
    if (index < security_locked_.size() && security_locked_[index]) {
        registers_.set_write_enable(false);
        registers_.set_erase_fail(true);
        return result(false, "SECURITY_ERASE rejected: locked");
    }
    account_command("security_erase");
    const std::uint64_t start = security_offset(index, 0);
    std::fill(security_.begin() + start,
              security_.begin() + start + security_register_size(),
              0xFF);
    registers_.set_write_enable(false);
    registers_.set_erase_fail(false);
    timing_.start_busy(config_.timing.sector_erase_us);
    return result(true, "SECURITY_ERASE accepted");
}

OperationResult FlashModel::program_security_register(std::uint8_t index,
                                                      std::uint16_t offset,
                                                      const std::vector<std::uint8_t>& data)
{
    if (is_nand_like(config_.device.cls)) return result(false, "SECURITY_PROGRAM rejected: NAND-like device");
    if (!config_.commands.program_security_register) return result(false, "SECURITY_PROGRAM unsupported");
    if (!config_.capabilities.security_register) return result(false, "SECURITY_PROGRAM rejected: capability disabled");
    if (!require_awake("SECURITY_PROGRAM")) return result(false, "SECURITY_PROGRAM rejected: deep power-down");
    if (!require_ready("SECURITY_PROGRAM")) return result(false, "SECURITY_PROGRAM rejected: busy");
    if (!require_not_suspended("SECURITY_PROGRAM")) return result(false, "SECURITY_PROGRAM rejected: suspended");
    if (!require_wel("SECURITY_PROGRAM")) return result(false, "SECURITY_PROGRAM rejected: WEL=0");
    if (!security_range_ok(index, offset, data.size())) return result(false, "SECURITY_PROGRAM rejected: range");
    if (index < security_locked_.size() && security_locked_[index]) {
        registers_.set_write_enable(false);
        registers_.set_program_fail(true);
        return result(false, "SECURITY_PROGRAM rejected: locked");
    }
    account_command("security_program", data.size());
    const std::uint64_t start = security_offset(index, offset);
    for (std::size_t i = 0; i < data.size(); ++i) {
        security_[start + i] = static_cast<std::uint8_t>(security_[start + i] & data[i]);
    }
    registers_.set_write_enable(false);
    registers_.set_program_fail(false);
    timing_.start_busy(config_.timing.program_us);
    return result(true, "SECURITY_PROGRAM accepted");
}

OperationResult FlashModel::lock_security_register(std::uint8_t index)
{
    if (is_nand_like(config_.device.cls)) return result(false, "SECURITY_LOCK rejected: NAND-like device");
    if (!config_.commands.lock_security_register) return result(false, "SECURITY_LOCK unsupported");
    if (!config_.capabilities.security_register) return result(false, "SECURITY_LOCK rejected: capability disabled");
    if (!require_awake("SECURITY_LOCK")) return result(false, "SECURITY_LOCK rejected: deep power-down");
    if (!require_ready("SECURITY_LOCK")) return result(false, "SECURITY_LOCK rejected: busy");
    if (!require_wel("SECURITY_LOCK")) return result(false, "SECURITY_LOCK rejected: WEL=0");
    if (index >= security_locked_.size()) return result(false, "SECURITY_LOCK rejected: range");
    account_command("security_lock");
    security_locked_[index] = true;
    registers_.set_write_enable(false);
    timing_.start_busy(config_.timing.program_us);
    return result(true, "SECURITY_LOCK accepted");
}

OperationResult FlashModel::enter_otp_mode()
{
    if (!is_nand_like(config_.device.cls)) return result(false, "ENTER_OTP rejected: NOR device");
    if (!config_.commands.enter_otp_mode) return result(false, "ENTER_OTP unsupported");
    if (!config_.capabilities.otp) return result(false, "ENTER_OTP rejected: capability disabled");
    if (!require_ready("ENTER_OTP")) return result(false, "ENTER_OTP rejected: busy");
    if (config_.constraints.otp_page_count == 0) return result(false, "ENTER_OTP rejected: otp_page_count=0");
    account_command("enter_otp_mode");
    registers_.mutable_state().otp_mode = true;
    timing_.advance(0.1);
    return result(true, "ENTER_OTP");
}

OperationResult FlashModel::exit_otp_mode()
{
    if (!is_nand_like(config_.device.cls)) return result(false, "EXIT_OTP rejected: NOR device");
    if (!config_.commands.exit_otp_mode) return result(false, "EXIT_OTP unsupported");
    if (!config_.capabilities.otp) return result(false, "EXIT_OTP rejected: capability disabled");
    if (!require_ready("EXIT_OTP")) return result(false, "EXIT_OTP rejected: busy");
    account_command("exit_otp_mode");
    registers_.mutable_state().otp_mode = false;
    timing_.advance(0.1);
    return result(true, "EXIT_OTP");
}

OperationResult FlashModel::get_feature(std::uint8_t address)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "GET_FEATURE rejected: NOR device");
    account_command("get_feature");
    const bool is_busy = timing_.busy();
    return result(true, "GET_FEATURE " + hex_byte(address),
                  {registers_.get_feature(address, is_busy)});
}

OperationResult FlashModel::set_feature(std::uint8_t address, std::uint8_t value)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "SET_FEATURE rejected: NOR device");
    if (!require_ready("SET_FEATURE")) return result(false, "SET_FEATURE rejected: busy");
    account_command("set_feature");
    registers_.set_feature(address, value);
    return result(true, "SET_FEATURE " + hex_byte(address) + "=" + hex_byte(value));
}

OperationResult FlashModel::select_die(std::uint32_t die)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "DIE_SELECT rejected: NOR device");
    if (!config_.capabilities.die_select) return result(false, "DIE_SELECT rejected: capability disabled");
    if (!require_ready("DIE_SELECT")) return result(false, "DIE_SELECT rejected: busy");
    if (die >= config_.geometry.dies) return result(false, "DIE_SELECT rejected: die out of range");
    account_command("select_die");
    registers_.mutable_state().active_die = die;
    timing_.advance(0.1);
    return result(true, "DIE_SELECT");
}

OperationResult FlashModel::select_plane(std::uint32_t plane)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "PLANE_SELECT rejected: NOR device");
    if (!config_.capabilities.plane_select) return result(false, "PLANE_SELECT rejected: capability disabled");
    if (!require_ready("PLANE_SELECT")) return result(false, "PLANE_SELECT rejected: busy");
    if (plane >= config_.geometry.planes) return result(false, "PLANE_SELECT rejected: plane out of range");
    account_command("select_plane");
    registers_.mutable_state().active_plane = plane;
    timing_.advance(0.1);
    return result(true, "PLANE_SELECT");
}

OperationResult FlashModel::set_read_retry_level(std::uint32_t level)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "READ_RETRY rejected: NOR device");
    if (!config_.capabilities.read_retry) return result(false, "READ_RETRY rejected: capability disabled");
    if (!require_ready("READ_RETRY")) return result(false, "READ_RETRY rejected: busy");
    if (config_.constraints.read_retry_levels == 0) {
        return result(false, "READ_RETRY rejected: no retry levels configured");
    }
    if (level >= config_.constraints.read_retry_levels) {
        return result(false, "READ_RETRY rejected: level out of range");
    }
    account_command("read_retry");
    registers_.mutable_state().read_retry_level = level;
    timing_.advance(0.1);
    return result(true, "READ_RETRY");
}

OperationResult FlashModel::block_erase(std::uint32_t block)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "BLOCK_ERASE rejected: NOR device");
    if (!config_.commands.block_erase) return result(false, "BLOCK_ERASE unsupported");
    if (!require_ready("BLOCK_ERASE")) return result(false, "BLOCK_ERASE rejected: busy");
    if (!require_not_suspended("BLOCK_ERASE")) return result(false, "BLOCK_ERASE rejected: suspended");
    if (registers_.state().otp_mode) return result(false, "BLOCK_ERASE rejected: OTP mode");
    if (!require_wel("BLOCK_ERASE")) return result(false, "BLOCK_ERASE rejected: WEL=0");
    if (block >= config_.geometry.blocks) return result(false, "BLOCK_ERASE rejected: block out of range");
    const CapabilityDecision capability = before_nand_block_erase(block);
    if (!capability.ok) {
        registers_.set_erase_fail(true);
        registers_.set_write_enable(false);
        return result(false, capability.message);
    }

    account_command("block_erase");
    storage_.erase(address_.nand_block_offset(block),
                   address_.nand_block_size_bytes());
    for (std::uint32_t i = 0; i < config_.geometry.pages_per_block; ++i) {
        partial_program_count_.erase(block * config_.geometry.pages_per_block + i);
    }
    next_page_by_block_[block] = block * config_.geometry.pages_per_block;
    registers_.set_write_enable(false);
    registers_.set_erase_fail(false);
    timing_.start_busy(config_.timing.block_erase_us);
    return result(true, "BLOCK_ERASE accepted");
}

OperationResult FlashModel::program_load(std::uint16_t column,
                                         const std::vector<std::uint8_t>& data,
                                         bool random_load)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "PROGRAM_LOAD rejected: NOR device");
    if (!config_.commands.program_load) return result(false, "PROGRAM_LOAD unsupported");
    if (!require_ready("PROGRAM_LOAD")) return result(false, "PROGRAM_LOAD rejected: busy");
    if (!require_not_suspended("PROGRAM_LOAD")) return result(false, "PROGRAM_LOAD rejected: suspended");
    if (!cache_valid_ || !random_load) {
        cache_.assign(config_.effective_page_size(), 0xFF);
        cache_valid_ = true;
        cache_program_violation_ = false;
    }
    if (column + data.size() > cache_.size()) return result(false, "PROGRAM_LOAD rejected: column out of range");
    account_command("program_load", data.size());
    std::copy(data.begin(), data.end(), cache_.begin() + column);
    if (marks_nand_program_load_violation(column, data.size())) cache_program_violation_ = true;
    return result(true, random_load ? "RANDOM_PROGRAM_LOAD" : "PROGRAM_LOAD");
}

bool FlashModel::otp_page_ok(std::uint32_t page) const
{
    return config_.constraints.otp_page_count != 0 &&
           page < config_.constraints.otp_page_count;
}

OperationResult FlashModel::execute_cached_program(std::uint32_t page,
                                                   const std::string& op,
                                                   const std::string& accepted_message)
{
    if (!cache_valid_) return result(false, op + " rejected: cache invalid");
    const bool otp_mode = registers_.state().otp_mode;
    const std::uint32_t total_pages = otp_mode
                                          ? config_.constraints.otp_page_count
                                          : address_.nand_total_pages();
    if (page >= total_pages) return result(false, op + " rejected: page out of range");

    const std::uint32_t block = otp_mode ? 0 : address_.nand_block_of_page(page);
    if (!otp_mode) {
        const CapabilityDecision capability = before_nand_program_execute(page, block);
        if (!capability.ok) {
            registers_.set_program_fail(true);
            registers_.set_write_enable(false);
            return result(false, capability.message);
        }
    }
    auto& program_count = otp_mode ? otp_program_count_ : partial_program_count_;
    const auto count_it = program_count.find(page);
    const std::uint32_t current_count = count_it == program_count.end() ? 0 : count_it->second;
    if (current_count >= config_.constraints.max_partial_programs) {
        registers_.set_program_fail(true);
        registers_.set_write_enable(false);
        return result(false, op + " rejected: partial program limit");
    }

    if (!otp_mode && config_.constraints.strict_sequential_program) {
        const std::uint32_t default_next = address_.nand_first_page_of_block(block);
        const std::uint32_t expected = next_page_by_block_.count(block) ? next_page_by_block_[block] : default_next;
        if (page > expected || (page < expected && current_count == 0)) {
            registers_.set_program_fail(true);
            registers_.set_write_enable(false);
            return result(false, op + " rejected: sequential program order");
        }
        if (page == expected) next_page_by_block_[block] = expected + 1;
    }

    if (otp_mode) {
        if (!otp_page_ok(page)) return result(false, op + " rejected: OTP page out of range");
        otp_storage_.program_and(static_cast<std::uint64_t>(page) *
                                 config_.effective_page_size(),
                                 cache_);
    } else {
        storage_.program_and(address_.nand_page_offset(page), cache_);
    }
    program_count[page] = current_count + 1;
    registers_.set_write_enable(false);
    registers_.set_program_fail(false);
    timing_.start_busy(config_.timing.program_us);
    return result(true, accepted_message);
}

OperationResult FlashModel::program_execute(std::uint32_t page)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "PROGRAM_EXECUTE rejected: NOR device");
    if (!config_.commands.program_execute) return result(false, "PROGRAM_EXECUTE unsupported");
    if (!require_ready("PROGRAM_EXECUTE")) return result(false, "PROGRAM_EXECUTE rejected: busy");
    if (!require_not_suspended("PROGRAM_EXECUTE")) return result(false, "PROGRAM_EXECUTE rejected: suspended");
    if (!require_wel("PROGRAM_EXECUTE")) return result(false, "PROGRAM_EXECUTE rejected: WEL=0");
    account_command("program_execute");
    return execute_cached_program(page, "PROGRAM_EXECUTE", "PROGRAM_EXECUTE accepted");
}

OperationResult FlashModel::copy_back(std::uint32_t source_page, std::uint32_t target_page)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "COPY_BACK rejected: NOR device");
    if (!config_.commands.copy_back) return result(false, "COPY_BACK unsupported");
    if (!config_.capabilities.copy_back) return result(false, "COPY_BACK rejected: capability disabled");
    if (!require_ready("COPY_BACK")) return result(false, "COPY_BACK rejected: busy");
    if (!require_not_suspended("COPY_BACK")) return result(false, "COPY_BACK rejected: suspended");
    if (registers_.state().otp_mode) return result(false, "COPY_BACK rejected: OTP mode");
    if (!require_wel("COPY_BACK")) return result(false, "COPY_BACK rejected: WEL=0");

    const std::uint32_t total_pages = address_.nand_total_pages();
    if (source_page >= total_pages) return result(false, "COPY_BACK rejected: source page out of range");
    if (target_page >= total_pages) return result(false, "COPY_BACK rejected: target page out of range");

    const std::uint32_t target_block = address_.nand_block_of_page(target_page);
    const CapabilityDecision capability =
        before_nand_copy_back(source_page, target_page, target_block);
    if (!capability.ok) {
        registers_.set_program_fail(true);
        registers_.set_write_enable(false);
        return result(false, capability.message);
    }

    cache_ = storage_.read(address_.nand_page_offset(source_page),
                           config_.effective_page_size());
    cache_valid_ = true;
    cache_program_violation_ = false;
    account_command("copy_back");
    return execute_cached_program(target_page, "COPY_BACK", "COPY_BACK accepted");
}

OperationResult FlashModel::page_read(std::uint32_t page)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "PAGE_READ rejected: NOR device");
    if (!config_.commands.page_read) return result(false, "PAGE_READ unsupported");
    if (!require_ready("PAGE_READ")) return result(false, "PAGE_READ rejected: busy");
    const bool otp_mode = registers_.state().otp_mode;
    const std::uint32_t total_pages = otp_mode
                                          ? config_.constraints.otp_page_count
                                          : address_.nand_total_pages();
    if (page >= total_pages) return result(false, "PAGE_READ rejected: page out of range");
    if (otp_mode) {
        cache_ = otp_storage_.read(static_cast<std::uint64_t>(page) *
                                   config_.effective_page_size(),
                                   config_.effective_page_size());
    } else {
        cache_ = storage_.read(address_.nand_page_offset(page), config_.effective_page_size());
    }
    cache_valid_ = true;
    account_command("page_read");
    timing_.start_busy(config_.timing.page_read_us);
    return result(true, "PAGE_READ accepted");
}

OperationResult FlashModel::read_from_cache(std::uint16_t column, std::size_t length)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "READ_FROM_CACHE rejected: NOR device");
    if (!config_.commands.read_from_cache) return result(false, "READ_FROM_CACHE unsupported");
    if (!require_ready("READ_FROM_CACHE")) return result(false, "READ_FROM_CACHE rejected: busy");
    if (!cache_valid_) return result(false, "READ_FROM_CACHE rejected: cache invalid");
    if (column + length > cache_.size()) return result(false, "READ_FROM_CACHE rejected: column out of range");
    std::vector<std::uint8_t> out(cache_.begin() + column, cache_.begin() + column + length);
    account_command("read_from_cache", 0, out.size());
    return result(true, "READ_FROM_CACHE", out);
}

OperationResult FlashModel::read_parameter_page(std::uint16_t offset, std::size_t length)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "READ_PARAMETER_PAGE rejected: NOR device");
    if (!config_.commands.read_parameter_page) return result(false, "READ_PARAMETER_PAGE unsupported");
    if (!config_.capabilities.parameter_page) return result(false, "READ_PARAMETER_PAGE rejected: capability disabled");
    if (!require_ready("READ_PARAMETER_PAGE")) return result(false, "READ_PARAMETER_PAGE rejected: busy");
    if (offset > parameter_page_.size() || length > parameter_page_.size() - offset) {
        return result(false, "READ_PARAMETER_PAGE rejected: range");
    }
    std::vector<std::uint8_t> out(parameter_page_.begin() + offset,
                                  parameter_page_.begin() + offset + length);
    account_command("read_parameter_page", 0, out.size());
    timing_.advance(config_.timing.read_us);
    return result(true, "READ_PARAMETER_PAGE", out);
}

} // namespace flash_model
