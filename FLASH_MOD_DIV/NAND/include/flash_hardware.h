#ifndef FLASH_MOD_FLASH_HARDWARE_H
#define FLASH_MOD_FLASH_HARDWARE_H

#include "addr_mapping.h"
#include "flash_config.h"
#include "storage_backend.h"

#include <cstdint>
#include <string>
#include <vector>

namespace flashmod {

// 每个 NAND page 的轻量元数据。数据本体在 StorageBackend 里，这里只记录
// 是否被编程过以及 partial program 次数。
struct PageMeta {
    bool valid = false;
    std::uint32_t program_count = 0;
};

// 每个 NAND block 的管理信息。factory_bad/permanent_locked 预留给坏块表、
// OTP/lock 等后续策略；next_program_page 用来检查顺序编程限制。
struct BlockMeta {
    bool factory_bad = false;
    bool permanent_locked = false;
    std::uint32_t next_program_page = 0;
    std::vector<PageMeta> pages;
};

// FlashHardware 是“硬件存储区”层：它组合 AddressMapper 和 StorageBackend，
// 管 page/block 元数据与文件 offset，但不解释命令语义、WEL、WIP 或状态寄存器。
class FlashHardware {
public:
    explicit FlashHardware(const DeviceProfile& profile);

    // 初始化 NAND block/page 元数据、可选 security register 和后端 storage 文件。
    void initialize();
    void initialize_storage();

    // 地址映射器对 FlashCore 只读开放；非 const 版本预留给后续高级映射策略。
    const AddressMapper& mapper() const { return mapper_; }
    AddressMapper& mapper() { return mapper_; }

    // NAND 几何检查与 block 元数据访问。
    bool valid_block(std::uint32_t block) const;
    bool valid_page(std::uint32_t page) const;
    const BlockMeta& block(std::uint32_t index) const;
    BlockMeta& block(std::uint32_t index);

    // NAND 编程约束：partial program 次数和 strict sequential program 都在这里检查。
    bool page_program_allowed(std::uint32_t block,
                              std::uint32_t page_in_block,
                              std::uint32_t max_partial_programs,
                              bool strict_sequential) const;
    void note_page_program(std::uint32_t block, std::uint32_t page_in_block);
    void reset_block_metadata(std::uint32_t block);

    // 存储原语。这里的 write 是“物理写入镜像文件”，不会自动做 old & new；
    // old & new 规则由 FlashCore 在 program 完成时显式处理。
    std::vector<std::uint8_t> read_bytes(std::uint64_t address, std::size_t length);
    void write_bytes(std::uint64_t address, const std::vector<std::uint8_t>& data);
    void erase_range(std::uint64_t address, std::uint64_t size);
    std::vector<std::uint8_t> read_page(std::uint32_t page);
    void write_page(std::uint32_t page, const std::vector<std::uint8_t>& data);
    void erase_block(std::uint32_t block);

    // 目前 security register 还没有完整命令路径，但 storage 区域已经由配置初始化。
    std::vector<std::vector<std::uint8_t> >& security_registers() { return security_registers_; }
    const std::vector<std::vector<std::uint8_t> >& security_registers() const { return security_registers_; }

private:
    // profile_ 由外部 FlashCore 持有，本类只保存引用；FlashHardware 生命周期必须短于 profile_。
    const DeviceProfile& profile_;
    AddressMapper mapper_;
    StorageBackend storage_;
    std::vector<BlockMeta> blocks_;
    std::vector<std::vector<std::uint8_t> > security_registers_;
};

} // namespace flashmod

#endif
