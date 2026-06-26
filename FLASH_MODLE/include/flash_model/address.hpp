#ifndef FLASH_MODEL_ADDRESS_HPP
#define FLASH_MODEL_ADDRESS_HPP

#include "flash_model/config.hpp"

#include <cstdint>

namespace flash_model {

// AddressMapper 只处理纯地址/几何换算，不执行读写，也不感知具体命令。
class AddressMapper {
public:
    explicit AddressMapper(const ModelConfig& config);

    // NOR 地址换算：把任意地址对齐到 sector/32KiB block/大 block 起点。
    std::uint64_t nor_sector_base(std::uint64_t address) const;
    std::uint64_t nor_block32_base(std::uint64_t address) const;
    std::uint64_t nor_block_base(std::uint64_t address) const;
    bool nor_range_protected(std::uint64_t address, std::uint64_t length) const;

    // NAND 地址换算：page/block 编号与线性存储后端 offset 之间转换。
    std::uint32_t nand_total_pages() const;
    std::uint32_t nand_block_of_page(std::uint32_t page) const;
    std::uint32_t nand_first_page_of_block(std::uint32_t block) const;
    std::uint64_t nand_page_offset(std::uint32_t page) const;
    std::uint64_t nand_block_offset(std::uint32_t block) const;
    std::uint64_t nand_block_size_bytes() const;

    // 半开区间 overlap 判断，统一给 NOR 保护区和 NAND reserved range 复用。
    static bool ranges_overlap(std::uint64_t a_start, std::uint64_t a_length,
                               std::uint64_t b_start, std::uint64_t b_length);

private:
    ModelConfig config_; // 保存配置快照，避免调用方生命周期影响地址换算。
};

} // namespace flash_model

#endif
