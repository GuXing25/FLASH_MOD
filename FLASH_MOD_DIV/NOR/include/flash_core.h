#ifndef FLASH_MOD_FLASH_CORE_H
#define FLASH_MOD_FLASH_CORE_H

#include "flash_config.h"
#include "flash_event.h"
#include "flash_hardware.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace flashmod {

// FlashCore 是器件行为核心：它不解析配置文件，也不直接暴露 storage 文件，
// 只根据 DeviceProfile 执行命令级语义。NOR 和 SPI-NAND 的共同规则
// （WEL/WIP、擦除态 0xFF、program 只能 1->0、busy 时间）都集中在这里。
class FlashCore {
public:
    explicit FlashCore(const DeviceProfile& profile);
    ~FlashCore();

    // 轻量状态查询。is_busy() 会先尝试完成已经到期的 pending 操作。
    const DeviceProfile& profile() const { return profile_; }
    double time_us() const { return now_us_; }
    bool is_busy();

    // 通用控制命令。
    OperationResult write_enable();
    OperationResult write_disable();
    OperationResult reset();
    OperationResult wait_us(double delta_us);
    OperationResult wait_ready();

    std::uint8_t status(std::uint8_t index);
    OperationResult read_id();

    // NOR 风格直接阵列命令：地址是 byte address，数据直接来自存储阵列。
    OperationResult read(std::uint64_t address, std::size_t length, IoMode mode = IoMode::X1);
    OperationResult program(std::uint64_t address, const std::vector<std::uint8_t>& data, IoMode mode = IoMode::X1);
    OperationResult erase(std::uint64_t address, EraseKind kind);

    // SPI-NAND 风格两阶段数据路径：page_read/program_execute 触发内部 busy，
    // read_from_cache/program_load 只访问 cache register。
    OperationResult get_feature(std::uint8_t address);
    OperationResult set_feature(std::uint8_t address, std::uint8_t value);
    OperationResult page_read(std::uint32_t page_address);
    OperationResult read_from_cache(std::uint16_t column, std::size_t length, IoMode mode = IoMode::X1);
    OperationResult program_load(std::uint16_t column,
                                 const std::vector<std::uint8_t>& data,
                                 bool random_load = false,
                                 IoMode mode = IoMode::X1);
    OperationResult program_execute(std::uint32_t page_address);
    OperationResult block_erase(std::uint32_t block_address);

private:
    // 真实 Flash 的 program/erase/read-page 并不是立即完成；这里用 PendingOp
    // 记录内部操作，wait_ready()/wait_us()/auto_complete 决定何时真正落盘。
    enum class PendingKind {
        None,
        NorProgram,
        NorErase,
        NandPageRead,
        NandProgram,
        NandErase,
        Reset
    };

    // pending_.data 对于 NOR program 是一整页待写数据，valid 标记这一页中
    // 哪些 byte 被本次命令覆盖；对于 NAND program，data 是 cache 的完整页镜像。
    struct PendingOp {
        PendingKind kind = PendingKind::None;
        double complete_time_us = 0.0;
        std::uint64_t address = 0;
        std::uint64_t size = 0;
        std::uint32_t page = 0;
        std::uint32_t block = 0;
        std::vector<std::uint8_t> data;
        std::vector<std::uint8_t> valid;
    };

    // profile_ 是配置画像；hardware_ 只负责底层阵列、地址映射和页/块元数据。
    DeviceProfile profile_;
    FlashHardware hardware_;

    // now_us_ 是模型内部单调时间。power_on_until_us_ 用来实现上电写保护窗口 tPUW。
    double now_us_ = 0.0;
    double power_on_until_us_ = 0.0;

    // NOR 使用 SR1/SR2/SR3 表示 WIP/WEL/QE 等；SPI-NAND 主要把状态同步到
    // feature register 0xC0。wel_/wip_ 是跨类型的内部规范化状态。
    std::uint8_t sr1_ = 0;
    std::uint8_t sr2_ = 0;
    std::uint8_t sr3_ = 0;
    bool wel_ = false;
    bool wip_ = false;
    bool deep_power_down_ = false;
    bool program_ecc_area_violation_ = false;

    // SPI-NAND feature registers 和 cache register。cache_valid_ 表示 cache
    // 当前是否来自 page_read 或 program_load，可被 read_from_cache/program_execute 使用。
    std::map<std::uint8_t, std::uint8_t> features_;
    std::vector<std::uint8_t> cache_;
    bool cache_valid_ = false;
    std::uint32_t cached_page_ = 0;
    PendingOp pending_;

    void initialize_registers();

    // 公共辅助：构造结果、推进时间、启动/完成内部 busy 操作。
    OperationResult make_result(bool ok, const std::string& message,
                                double bus = 0.0, double internal = 0.0,
                                const std::vector<std::uint8_t>& data = std::vector<std::uint8_t>());
    void advance_time(double delta_us);
    void start_pending(PendingOp op, double internal_us);
    void complete_pending_if_ready();
    void complete_pending_operation();

    // 前置条件检查和状态位同步。
    bool require_ready(const char* op);
    bool require_write_enable(const char* op);
    bool require_quad_enable(const char* op, IoMode mode);
    void set_wel(bool value);
    void set_wip(bool value);
    void set_program_fail(bool value);
    void set_erase_fail(bool value);
    void clear_fail_bits();

    // 具体规则：block protection、总线耗时、典型/最大 program 时间、擦除时间和 ECC reserved 区判断。
    bool is_block_protected(std::uint32_t block) const;
    double bus_time_for_bytes(std::size_t command_bits, std::size_t data_bytes, IoMode mode) const;
    double program_time() const;
    double erase_time_for_kind(EraseKind kind) const;
    bool touches_ecc_reserved(std::uint16_t column, std::size_t length) const;
};

} // namespace flashmod

#endif
