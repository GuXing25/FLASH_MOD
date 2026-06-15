#ifndef FLASH_MOD_ADDR_MAPPING_H
#define FLASH_MOD_ADDR_MAPPING_H

#include "flash_config.h"

#include <cstdint>

namespace flashmod {

// NAND 行地址解码结果。page 是线性页号，block/page_in_block/plane 是从
// DeviceProfile 几何结构推导出来的层级地址。
struct NandAddress {
    std::uint32_t page = 0;
    std::uint32_t block = 0;
    std::uint32_t page_in_block = 0;
    std::uint32_t plane = 0;
    std::uint64_t byte_offset = 0;
};

// AddressMapper 只做地址归一化和几何映射，不访问 storage，也不判断命令是否合法。
// 这样 NOR byte address 和 NAND page/block address 的计算可以被 FlashCore/Hardware 共用。
class AddressMapper {
public:
    explicit AddressMapper(const DeviceProfile& profile);

    // NOR byte address 工具：根据 WRAP_ADDRESS 决定是否环回，并支持擦除对齐。
    std::uint64_t normalize_array_address(std::uint64_t address) const;
    bool valid_array_range(std::uint64_t address, std::uint64_t length) const;
    std::uint64_t align_down(std::uint64_t address, std::uint64_t size) const;

    // NAND 几何映射：线性 page/block 到镜像文件 offset，以及 page/block 的合法性判断。
    std::uint64_t page_offset(std::uint32_t page) const;
    std::uint64_t block_offset(std::uint32_t block) const;
    std::uint64_t block_size_bytes() const;
    std::uint32_t block_of_page(std::uint32_t page) const;
    std::uint32_t page_in_block(std::uint32_t page) const;
    std::uint32_t plane_of_block(std::uint32_t block) const;
    bool valid_page(std::uint32_t page) const;
    bool valid_block(std::uint32_t block) const;
    NandAddress decode_page(std::uint32_t page) const;

private:
    const DeviceProfile& profile_;
};

} // namespace flashmod

#endif
