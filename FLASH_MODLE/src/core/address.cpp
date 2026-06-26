#include "flash_model/address.hpp"

#include <algorithm>
#include <limits>

namespace flash_model {
namespace {

std::uint64_t saturating_end(std::uint64_t start, std::uint64_t length)
{
    const std::uint64_t max = std::numeric_limits<std::uint64_t>::max();
    if (length > max - start) return max;
    return start + length;
}

} // namespace

AddressMapper::AddressMapper(const ModelConfig& config) : config_(config) {}

std::uint64_t AddressMapper::nor_sector_base(std::uint64_t address) const
{
    const std::uint64_t sector = config_.geometry.sector_size;
    if (sector == 0) return address;
    return (address / sector) * sector;
}

std::uint64_t AddressMapper::nor_block32_base(std::uint64_t address) const
{
    const std::uint64_t block = config_.geometry.block32_size;
    if (block == 0) return address;
    return (address / block) * block;
}

std::uint64_t AddressMapper::nor_block_base(std::uint64_t address) const
{
    const std::uint64_t block = config_.geometry.block_size;
    if (block == 0) return address;
    return (address / block) * block;
}

bool AddressMapper::nor_range_protected(std::uint64_t address, std::uint64_t length) const
{
    if (!config_.capabilities.block_protect) return false;
    if (config_.constraints.nor_protect_length == 0 || length == 0) return false;
    return ranges_overlap(address, length,
                          config_.constraints.nor_protect_start,
                          config_.constraints.nor_protect_length);
}

std::uint32_t AddressMapper::nand_total_pages() const
{
    return config_.geometry.blocks * config_.geometry.pages_per_block;
}

std::uint32_t AddressMapper::nand_block_of_page(std::uint32_t page) const
{
    if (config_.geometry.pages_per_block == 0) return 0;
    return page / config_.geometry.pages_per_block;
}

std::uint32_t AddressMapper::nand_first_page_of_block(std::uint32_t block) const
{
    return block * config_.geometry.pages_per_block;
}

std::uint64_t AddressMapper::nand_page_offset(std::uint32_t page) const
{
    return static_cast<std::uint64_t>(page) * config_.effective_page_size();
}

std::uint64_t AddressMapper::nand_block_offset(std::uint32_t block) const
{
    return static_cast<std::uint64_t>(nand_first_page_of_block(block)) *
           config_.effective_page_size();
}

std::uint64_t AddressMapper::nand_block_size_bytes() const
{
    return static_cast<std::uint64_t>(config_.geometry.pages_per_block) *
           config_.effective_page_size();
}

bool AddressMapper::ranges_overlap(std::uint64_t a_start, std::uint64_t a_length,
                                   std::uint64_t b_start, std::uint64_t b_length)
{
    if (a_length == 0 || b_length == 0) return false;
    const std::uint64_t a_end = saturating_end(a_start, a_length);
    const std::uint64_t b_end = saturating_end(b_start, b_length);
    return a_start < b_end && b_start < a_end;
}

} // namespace flash_model
