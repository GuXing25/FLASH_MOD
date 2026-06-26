#ifndef FLASH_MODEL_ADDRESS_HPP
#define FLASH_MODEL_ADDRESS_HPP

#include "flash_model/config.hpp"

#include <cstdint>

namespace flash_model {

// Maps logical NOR/NAND addresses to array offsets.
class AddressMapper {
public:
    explicit AddressMapper(const ModelConfig& config);

    std::uint64_t nor_sector_base(std::uint64_t address) const;
    std::uint64_t nor_block32_base(std::uint64_t address) const;
    std::uint64_t nor_block_base(std::uint64_t address) const;
    bool nor_range_protected(std::uint64_t address, std::uint64_t length) const;

    std::uint32_t nand_total_pages() const;
    std::uint32_t nand_block_of_page(std::uint32_t page) const;
    std::uint32_t nand_first_page_of_block(std::uint32_t block) const;
    std::uint64_t nand_page_offset(std::uint32_t page) const;
    std::uint64_t nand_block_offset(std::uint32_t block) const;
    std::uint64_t nand_block_size_bytes() const;

    static bool ranges_overlap(std::uint64_t a_start, std::uint64_t a_length,
                               std::uint64_t b_start, std::uint64_t b_length);

private:
    ModelConfig config_;
};

} // namespace flash_model

#endif
