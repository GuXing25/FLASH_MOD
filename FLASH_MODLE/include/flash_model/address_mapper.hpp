#ifndef FLASH_MODEL_ADDRESS_MAPPER_HPP
#define FLASH_MODEL_ADDRESS_MAPPER_HPP

#include "flash_model/config.hpp"

#include <cstdint>

namespace flash_model {

// AddressMapper 集中管理“逻辑地址如何落到阵列偏移”的规则。
//
// 第一版只覆盖最小公共映射：
// - NOR byte address 到 sector base。
// - NOR 保护窗口 overlap 判断。
// - SPI-NAND page/block 到 storage byte offset。
//
// 后续 plane_select、die_select、bad_block remap、copy-back 都会依赖地址规则。
// 因此地址公式不要再散写到 FlashModel 或 capability module 中，应优先放这里。
class AddressMapper {
public:
    explicit AddressMapper(const ModelConfig& config);

    std::uint64_t nor_sector_base(std::uint64_t address) const;
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
