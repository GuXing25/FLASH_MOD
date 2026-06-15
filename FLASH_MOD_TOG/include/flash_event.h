#ifndef FLASH_MOD_FLASH_EVENT_H
#define FLASH_MOD_FLASH_EVENT_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace flashmod {

// IoMode 描述一次命令的数据线宽，而不是具体 opcode。FlashCore 用它计算
// 总线传输时间，并在需要时检查 QE 位是否允许 Quad 类命令。
enum class IoMode {
    X1,
    X2,
    X4,
    DualIO,
    QuadIO,
    QuadDTR
};

// 统一的擦除粒度。NOR 会映射到 sector/block/chip erase；NAND 的 block erase
// 走独立的 BlockErase 事件，不使用这个枚举。
enum class EraseKind {
    Sector4K,
    Block32K,
    Block64K,
    Chip
};

// FTL 队列里的事件类型。这里的命名刻意保持“命令级”，让上层可以直接从
// 数据手册命令流程翻译成 FlashEvent 序列。
enum class EventType {
    WriteEnable,
    WriteDisable,
    Reset,
    WaitUs,
    WaitReady,
    ReadId,
    ReadStatus,

    NorRead,
    NorProgram,
    NorErase,

    GetFeature,
    SetFeature,
    PageRead,
    ReadFromCache,
    ProgramLoad,
    ProgramExecute,
    BlockErase
};

// 一次命令执行后的统一返回值。
// - ok/message 供测试和上层调度判断。
// - data 承载 read-id、read-cache、read-status 等返回数据。
// - bus_time_us/internal_time_us/now_us 供性能统计或时序仿真使用。
struct OperationResult {
    bool ok = true;
    std::string message;
    std::vector<std::uint8_t> data;
    double bus_time_us = 0.0;
    double internal_time_us = 0.0;
    double now_us = 0.0;
};

// FlashEvent 是 FTL 与 FlashCore 之间的统一请求对象。不同命令只使用其中
// 一部分字段，例如 NOR_READ 使用 address/length/mode，PROGRAM_LOAD 使用
// column/payload/random_load/mode。
struct FlashEvent {
    // 公共字段：未被当前事件使用的字段保持默认值。
    EventType type = EventType::WaitReady;
    std::uint64_t address = 0;
    std::uint16_t column = 0;
    std::uint8_t index = 0;
    std::uint8_t value = 0;
    std::size_t length = 0;
    double wait_us = 0.0;
    bool random_load = false;
    IoMode mode = IoMode::X1;
    EraseKind erase_kind = EraseKind::Sector4K;
    std::vector<std::uint8_t> payload;

    // 静态工厂函数把事件构造集中起来，避免调用方手动填字段时漏填 type 或填错字段。
    static FlashEvent write_enable();
    static FlashEvent write_disable();
    static FlashEvent reset();
    static FlashEvent wait_us_cmd(double us);
    static FlashEvent wait_ready();
    static FlashEvent read_id();
    static FlashEvent read_status(std::uint8_t index);
    static FlashEvent nor_read(std::uint64_t address, std::size_t length, IoMode mode = IoMode::X1);
    static FlashEvent nor_program(std::uint64_t address,
                                  const std::vector<std::uint8_t>& data,
                                  IoMode mode = IoMode::X1);
    static FlashEvent nor_erase(std::uint64_t address, EraseKind kind);
    static FlashEvent get_feature(std::uint8_t address);
    static FlashEvent set_feature(std::uint8_t address, std::uint8_t value);
    static FlashEvent page_read(std::uint32_t page);
    static FlashEvent read_from_cache(std::uint16_t column, std::size_t length, IoMode mode = IoMode::X1);
    static FlashEvent program_load(std::uint16_t column,
                                   const std::vector<std::uint8_t>& data,
                                   bool random_load = false,
                                   IoMode mode = IoMode::X1);
    static FlashEvent program_execute(std::uint32_t page);
    static FlashEvent block_erase(std::uint32_t block);
};

// 字符串化函数用于日志、自测输出和后续 trace 文件。
std::string to_string(IoMode mode);
std::string to_string(EraseKind kind);
std::string event_name(EventType type);

} // namespace flashmod

#endif
