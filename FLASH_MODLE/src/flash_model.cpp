#include "flash_model/flash_model.hpp"

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
      address_mapper_(config_),
      storage_(config_.total_size_bytes())
{
    for (const auto& module : modules_) module->attach(registers_.mutable_state());
    if (policy_) policy_->on_power_on(registers_.mutable_state());
    if (config_.effective_page_size() != 0) {
        cache_.assign(config_.effective_page_size(), 0xFF);
    }
}

OperationResult FlashModel::result(bool ok,
                                   const std::string& message,
                                   const std::vector<std::uint8_t>& data)
{
    return OperationResult{ok, message, data, timing_.now_us()};
}

bool FlashModel::busy()
{
    return timing_.busy();
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

OperationResult FlashModel::read_id()
{
    if (!config_.commands.read_id) return result(false, "READ_ID unsupported");
    if (!require_ready("READ_ID")) return result(false, "READ_ID rejected: busy");
    timing_.advance(0.1);
    return result(true, "READ_ID", config_.device.id);
}

OperationResult FlashModel::write_enable()
{
    if (!require_ready("WRITE_ENABLE")) return result(false, "WRITE_ENABLE rejected: busy");
    registers_.set_write_enable(true);
    timing_.advance(0.1);
    return result(true, "WRITE_ENABLE: WEL=1");
}

OperationResult FlashModel::write_disable()
{
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
    registers_.reset();
    if (policy_) policy_->on_power_on(registers_.mutable_state());
    timing_.advance(config_.timing.reset_us);
    return result(true, "RESET");
}

OperationResult FlashModel::nor_read(std::uint64_t address, std::size_t length)
{
    if (is_nand_like(config_.device.cls)) return result(false, "NOR_READ rejected: NAND-like device");
    if (!config_.commands.nor_read) return result(false, "NOR_READ unsupported");
    if (!require_ready("NOR_READ")) return result(false, "NOR_READ rejected: busy");
    timing_.advance(config_.timing.read_us);
    return result(true, "NOR_READ", storage_.read(address, length));
}

OperationResult FlashModel::nor_program(std::uint64_t address, const std::vector<std::uint8_t>& data)
{
    if (is_nand_like(config_.device.cls)) return result(false, "NOR_PROGRAM rejected: NAND-like device");
    if (!config_.commands.nor_program) return result(false, "NOR_PROGRAM unsupported");
    if (!require_ready("NOR_PROGRAM")) return result(false, "NOR_PROGRAM rejected: busy");
    if (!require_wel("NOR_PROGRAM")) return result(false, "NOR_PROGRAM rejected: WEL=0");
    if (address_mapper_.nor_range_protected(address, data.size())) {
        registers_.set_write_enable(false);
        registers_.set_program_fail(true);
        return result(false, "NOR_PROGRAM rejected: protected range");
    }
    storage_.program_and(address, data);
    registers_.set_write_enable(false);
    registers_.set_program_fail(false);
    timing_.start_busy(config_.timing.program_us);
    return result(true, "NOR_PROGRAM accepted");
}

OperationResult FlashModel::nor_sector_erase(std::uint64_t address)
{
    if (is_nand_like(config_.device.cls)) return result(false, "NOR_ERASE rejected: NAND-like device");
    if (!config_.commands.nor_erase) return result(false, "NOR_ERASE unsupported");
    if (!require_ready("NOR_ERASE")) return result(false, "NOR_ERASE rejected: busy");
    if (!require_wel("NOR_ERASE")) return result(false, "NOR_ERASE rejected: WEL=0");
    const std::uint64_t sector = config_.geometry.sector_size;
    const std::uint64_t base = address_mapper_.nor_sector_base(address);
    if (address_mapper_.nor_range_protected(base, sector)) {
        registers_.set_write_enable(false);
        registers_.set_erase_fail(true);
        return result(false, "NOR_ERASE rejected: protected range");
    }
    storage_.erase(base, sector);
    registers_.set_write_enable(false);
    registers_.set_erase_fail(false);
    timing_.start_busy(config_.timing.sector_erase_us);
    return result(true, "NOR_SECTOR_ERASE accepted");
}

OperationResult FlashModel::get_feature(std::uint8_t address)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "GET_FEATURE rejected: NOR device");
    const bool is_busy = timing_.busy();
    return result(true, "GET_FEATURE " + hex_byte(address),
                  {registers_.get_feature(address, is_busy)});
}

OperationResult FlashModel::set_feature(std::uint8_t address, std::uint8_t value)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "SET_FEATURE rejected: NOR device");
    if (!require_ready("SET_FEATURE")) return result(false, "SET_FEATURE rejected: busy");
    registers_.set_feature(address, value);
    return result(true, "SET_FEATURE " + hex_byte(address) + "=" + hex_byte(value));
}

bool FlashModel::nand_block_locked(std::uint32_t) const
{
    if (!config_.capabilities.block_protect) return false;
    return (registers_.feature(0xA0) & 0x7Cu) != 0;
}

OperationResult FlashModel::block_erase(std::uint32_t block)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "BLOCK_ERASE rejected: NOR device");
    if (!config_.commands.block_erase) return result(false, "BLOCK_ERASE unsupported");
    if (!require_ready("BLOCK_ERASE")) return result(false, "BLOCK_ERASE rejected: busy");
    if (!require_wel("BLOCK_ERASE")) return result(false, "BLOCK_ERASE rejected: WEL=0");
    if (block >= config_.geometry.blocks) return result(false, "BLOCK_ERASE rejected: block out of range");
    if (nand_block_locked(block)) {
        registers_.set_erase_fail(true);
        registers_.set_write_enable(false);
        return result(false, "BLOCK_ERASE rejected: block protected");
    }

    storage_.erase(address_mapper_.nand_block_offset(block),
                   address_mapper_.nand_block_size_bytes());
    for (std::uint32_t i = 0; i < config_.geometry.pages_per_block; ++i) {
        partial_program_count_.erase(block * config_.geometry.pages_per_block + i);
    }
    next_page_by_block_[block] = block * config_.geometry.pages_per_block;
    registers_.set_write_enable(false);
    registers_.set_erase_fail(false);
    timing_.start_busy(config_.timing.block_erase_us);
    return result(true, "BLOCK_ERASE accepted");
}

bool FlashModel::touches_ecc_reserved(std::uint16_t column, std::size_t length) const
{
    if (!config_.capabilities.ecc_status) return false;
    if (config_.constraints.ecc_reserved_end == 0) return false;
    const std::uint32_t start = column;
    const std::uint32_t end = static_cast<std::uint32_t>(column + length);
    return start < config_.constraints.ecc_reserved_end &&
           end > config_.constraints.ecc_reserved_start;
}

OperationResult FlashModel::program_load(std::uint16_t column,
                                         const std::vector<std::uint8_t>& data,
                                         bool random_load)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "PROGRAM_LOAD rejected: NOR device");
    if (!config_.commands.program_load) return result(false, "PROGRAM_LOAD unsupported");
    if (!require_ready("PROGRAM_LOAD")) return result(false, "PROGRAM_LOAD rejected: busy");
    if (!cache_valid_ || !random_load) {
        cache_.assign(config_.effective_page_size(), 0xFF);
        cache_valid_ = true;
        cache_ecc_violation_ = false;
    }
    if (column + data.size() > cache_.size()) return result(false, "PROGRAM_LOAD rejected: column out of range");
    std::copy(data.begin(), data.end(), cache_.begin() + column);
    if (touches_ecc_reserved(column, data.size())) cache_ecc_violation_ = true;
    return result(true, random_load ? "RANDOM_PROGRAM_LOAD" : "PROGRAM_LOAD");
}

OperationResult FlashModel::program_execute(std::uint32_t page)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "PROGRAM_EXECUTE rejected: NOR device");
    if (!config_.commands.program_execute) return result(false, "PROGRAM_EXECUTE unsupported");
    if (!require_ready("PROGRAM_EXECUTE")) return result(false, "PROGRAM_EXECUTE rejected: busy");
    if (!require_wel("PROGRAM_EXECUTE")) return result(false, "PROGRAM_EXECUTE rejected: WEL=0");
    if (!cache_valid_) return result(false, "PROGRAM_EXECUTE rejected: cache invalid");
    const std::uint32_t total_pages = address_mapper_.nand_total_pages();
    if (page >= total_pages) return result(false, "PROGRAM_EXECUTE rejected: page out of range");

    const std::uint32_t block = address_mapper_.nand_block_of_page(page);
    if (nand_block_locked(block)) {
        registers_.set_program_fail(true);
        registers_.set_write_enable(false);
        return result(false, "PROGRAM_EXECUTE rejected: block protected");
    }
    if (cache_ecc_violation_) {
        registers_.set_program_fail(true);
        registers_.set_write_enable(false);
        return result(false, "PROGRAM_EXECUTE rejected: ECC reserved spare range");
    }

    const auto count_it = partial_program_count_.find(page);
    const std::uint32_t current_count = count_it == partial_program_count_.end() ? 0 : count_it->second;
    if (current_count >= config_.constraints.max_partial_programs) {
        registers_.set_program_fail(true);
        registers_.set_write_enable(false);
        return result(false, "PROGRAM_EXECUTE rejected: partial program limit");
    }

    if (config_.constraints.strict_sequential_program) {
        const std::uint32_t default_next = address_mapper_.nand_first_page_of_block(block);
        const std::uint32_t expected = next_page_by_block_.count(block) ? next_page_by_block_[block] : default_next;
        if (page > expected || (page < expected && current_count == 0)) {
            registers_.set_program_fail(true);
            registers_.set_write_enable(false);
            return result(false, "PROGRAM_EXECUTE rejected: sequential program order");
        }
        if (page == expected) next_page_by_block_[block] = expected + 1;
    }

    storage_.program_and(address_mapper_.nand_page_offset(page), cache_);
    partial_program_count_[page] = current_count + 1;
    registers_.set_write_enable(false);
    registers_.set_program_fail(false);
    timing_.start_busy(config_.timing.program_us);
    return result(true, "PROGRAM_EXECUTE accepted");
}

OperationResult FlashModel::page_read(std::uint32_t page)
{
    if (!is_nand_like(config_.device.cls)) return result(false, "PAGE_READ rejected: NOR device");
    if (!config_.commands.page_read) return result(false, "PAGE_READ unsupported");
    if (!require_ready("PAGE_READ")) return result(false, "PAGE_READ rejected: busy");
    const std::uint32_t total_pages = address_mapper_.nand_total_pages();
    if (page >= total_pages) return result(false, "PAGE_READ rejected: page out of range");
    cache_ = storage_.read(address_mapper_.nand_page_offset(page), config_.effective_page_size());
    cache_valid_ = true;
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
    return result(true, "READ_FROM_CACHE", out);
}

} // namespace flash_model
