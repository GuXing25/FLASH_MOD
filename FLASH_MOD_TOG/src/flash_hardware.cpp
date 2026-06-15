#include "flash_hardware.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace flashmod {

FlashHardware::FlashHardware(const DeviceProfile& profile)
    : profile_(profile), mapper_(profile)
{
}

void FlashHardware::initialize()
{
    if (profile_.is_nand_like()) {
        // NAND 需要按 block/page 保存编程次数和顺序状态；NOR 只需要线性 storage。
        blocks_.assign(profile_.effective_blocks(), BlockMeta());
        for (BlockMeta& block : blocks_) {
            block.pages.assign(profile_.pages_per_block, PageMeta());
        }
    }

    if (profile_.security_registers && profile_.security_register_count != 0) {
        // Security register 暂时只初始化镜像，具体命令可在 FlashCore 后续补充。
        security_registers_.assign(profile_.security_register_count,
                                   std::vector<std::uint8_t>(profile_.security_register_size, 0xFF));
    }

    initialize_storage();
}

void FlashHardware::initialize_storage()
{
    // STORAGE_FILE 可以是相对路径；相对路径默认落在配置文件所在目录，便于每个 profile
    // 自己维护独立镜像。
    std::filesystem::path path(profile_.storage_file);
    if (path.is_relative() && !profile_.config_dir.empty()) {
        path = std::filesystem::path(profile_.config_dir) / path;
    }
    storage_.open(path.string(), profile_.total_size_bytes(), profile_.reset_storage_on_start);
}

bool FlashHardware::valid_block(std::uint32_t block) const
{
    return mapper_.valid_block(block) && block < blocks_.size();
}

bool FlashHardware::valid_page(std::uint32_t page) const
{
    return mapper_.valid_page(page);
}

const BlockMeta& FlashHardware::block(std::uint32_t index) const
{
    if (!valid_block(index)) throw std::out_of_range("block out of range");
    return blocks_[index];
}

BlockMeta& FlashHardware::block(std::uint32_t index)
{
    if (!valid_block(index)) throw std::out_of_range("block out of range");
    return blocks_[index];
}

bool FlashHardware::page_program_allowed(std::uint32_t block_index,
                                         std::uint32_t page_in_block,
                                         std::uint32_t max_partial_programs,
                                         bool strict_sequential) const
{
    // 这里不检查 WEL/cache/ECC，只检查 NAND 阵列层面的编程约束。
    if (!valid_block(block_index)) return false;
    const BlockMeta& meta = blocks_[block_index];
    if (page_in_block >= meta.pages.size()) return false;
    const PageMeta& page = meta.pages[page_in_block];
    if (page.program_count >= max_partial_programs) return false;
    if (!strict_sequential) return true;
    // strict_sequential=true 时，只允许写 next_program_page 或更早页的 partial program。
    return page_in_block <= meta.next_program_page;
}

void FlashHardware::note_page_program(std::uint32_t block_index, std::uint32_t page_in_block)
{
    // 只有 program 真正完成后才更新元数据；命令被拒绝或仍 busy 时不改变 page meta。
    BlockMeta& meta = block(block_index);
    if (page_in_block >= meta.pages.size()) return;
    PageMeta& page = meta.pages[page_in_block];
    page.valid = true;
    ++page.program_count;
    if (page_in_block >= meta.next_program_page) meta.next_program_page = page_in_block + 1;
}

void FlashHardware::reset_block_metadata(std::uint32_t block_index)
{
    // block erase 后，页回到未编程状态，顺序编程游标也回到 block 开头。
    if (!valid_block(block_index)) return;
    BlockMeta& meta = blocks_[block_index];
    meta.next_program_page = 0;
    for (PageMeta& page : meta.pages) page = PageMeta();
}

std::vector<std::uint8_t> FlashHardware::read_bytes(std::uint64_t address, std::size_t length)
{
    return storage_.read(address, length, profile_.wrap_address, profile_.total_size_bytes());
}

void FlashHardware::write_bytes(std::uint64_t address, const std::vector<std::uint8_t>& data)
{
    storage_.write(address, data, profile_.wrap_address, profile_.total_size_bytes());
}

void FlashHardware::erase_range(std::uint64_t address, std::uint64_t size)
{
    storage_.erase(address, size, profile_.wrap_address, profile_.total_size_bytes());
}

std::vector<std::uint8_t> FlashHardware::read_page(std::uint32_t page)
{
    // NAND page 包含 main + spare；profile_.effective_page_size() 已经处理这点。
    return read_bytes(mapper_.page_offset(page), profile_.effective_page_size());
}

void FlashHardware::write_page(std::uint32_t page, const std::vector<std::uint8_t>& data)
{
    write_bytes(mapper_.page_offset(page), data);
}

void FlashHardware::erase_block(std::uint32_t block_index)
{
    erase_range(mapper_.block_offset(block_index), mapper_.block_size_bytes());
}

} // namespace flashmod
