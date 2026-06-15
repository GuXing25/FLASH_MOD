#include "addr_mapping.h"

#include <algorithm>

namespace flashmod {

AddressMapper::AddressMapper(const DeviceProfile& profile) : profile_(profile) {}

std::uint64_t AddressMapper::normalize_array_address(std::uint64_t address) const
{
    // NOR direct-array 命令使用 byte address。wrap_address=true 时模拟部分模型的地址回绕。
    const std::uint64_t size = profile_.total_size_bytes();
    if (size == 0) return 0;
    return profile_.wrap_address ? address % size : address;
}

bool AddressMapper::valid_array_range(std::uint64_t address, std::uint64_t length) const
{
    // 开启 wrap 时，任意起始地址都可以归一化；关闭 wrap 时必须完整落在阵列范围内。
    if (profile_.wrap_address) return true;
    return address <= profile_.total_size_bytes() && length <= profile_.total_size_bytes() - address;
}

std::uint64_t AddressMapper::align_down(std::uint64_t address, std::uint64_t size) const
{
    if (size == 0) return address;
    return address - (address % size);
}

std::uint64_t AddressMapper::page_offset(std::uint32_t page) const
{
    // 镜像文件中 NAND page 按线性页连续排列，每页包含 main+spare。
    return static_cast<std::uint64_t>(page) * profile_.effective_page_size();
}

std::uint64_t AddressMapper::block_offset(std::uint32_t block) const
{
    return static_cast<std::uint64_t>(block) * block_size_bytes();
}

std::uint64_t AddressMapper::block_size_bytes() const
{
    return static_cast<std::uint64_t>(profile_.pages_per_block) * profile_.effective_page_size();
}

std::uint32_t AddressMapper::block_of_page(std::uint32_t page) const
{
    return profile_.pages_per_block == 0 ? 0 : page / profile_.pages_per_block;
}

std::uint32_t AddressMapper::page_in_block(std::uint32_t page) const
{
    return profile_.pages_per_block == 0 ? 0 : page % profile_.pages_per_block;
}

std::uint32_t AddressMapper::plane_of_block(std::uint32_t block) const
{
    // 当前采用简单交织：block % planes。若后续需要厂商特殊 plane 编址，可集中改这里。
    const std::uint32_t planes = std::max<std::uint32_t>(profile_.planes, 1);
    return block % planes;
}

bool AddressMapper::valid_page(std::uint32_t page) const
{
    return page < profile_.total_pages();
}

bool AddressMapper::valid_block(std::uint32_t block) const
{
    return block < profile_.effective_blocks();
}

NandAddress AddressMapper::decode_page(std::uint32_t page) const
{
    // 调试和未来多 plane 调度可直接使用这个完整解码结果。
    NandAddress out;
    out.page = page;
    out.block = block_of_page(page);
    out.page_in_block = page_in_block(page);
    out.plane = plane_of_block(out.block);
    out.byte_offset = page_offset(page);
    return out;
}

} // namespace flashmod
