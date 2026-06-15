#include "flash_event.h"

namespace flashmod {

std::string to_string(IoMode mode)
{
    // 日志用小写/短横线格式，便于直接写入 trace 或 CSV。
    switch (mode) {
    case IoMode::X1: return "x1";
    case IoMode::X2: return "x2";
    case IoMode::X4: return "x4";
    case IoMode::DualIO: return "dual-io";
    case IoMode::QuadIO: return "quad-io";
    case IoMode::QuadDTR: return "quad-dtr";
    }
    return "unknown";
}

std::string to_string(EraseKind kind)
{
    switch (kind) {
    case EraseKind::Sector4K: return "sector";
    case EraseKind::Block32K: return "block32";
    case EraseKind::Block64K: return "block64";
    case EraseKind::Chip: return "chip";
    }
    return "unknown";
}

std::string event_name(EventType type)
{
    // 事件名保持接近数据手册命令名，方便调试时和命令序列对照。
    switch (type) {
    case EventType::WriteEnable: return "WRITE_ENABLE";
    case EventType::WriteDisable: return "WRITE_DISABLE";
    case EventType::Reset: return "RESET";
    case EventType::WaitUs: return "WAIT_US";
    case EventType::WaitReady: return "WAIT_READY";
    case EventType::ReadId: return "READ_ID";
    case EventType::ReadStatus: return "READ_STATUS";
    case EventType::NorRead: return "NOR_READ";
    case EventType::NorProgram: return "NOR_PROGRAM";
    case EventType::NorErase: return "NOR_ERASE";
    case EventType::GetFeature: return "GET_FEATURE";
    case EventType::SetFeature: return "SET_FEATURE";
    case EventType::PageRead: return "PAGE_READ";
    case EventType::ReadFromCache: return "READ_FROM_CACHE";
    case EventType::ProgramLoad: return "PROGRAM_LOAD";
    case EventType::ProgramExecute: return "PROGRAM_EXECUTE";
    case EventType::BlockErase: return "BLOCK_ERASE";
    }
    return "UNKNOWN";
}

FlashEvent FlashEvent::write_enable()
{
    // 无参数命令只需要设置 type，其他字段保留默认值。
    FlashEvent event;
    event.type = EventType::WriteEnable;
    return event;
}

FlashEvent FlashEvent::write_disable()
{
    FlashEvent event;
    event.type = EventType::WriteDisable;
    return event;
}

FlashEvent FlashEvent::reset()
{
    FlashEvent event;
    event.type = EventType::Reset;
    return event;
}

FlashEvent FlashEvent::wait_us_cmd(double us)
{
    FlashEvent event;
    event.type = EventType::WaitUs;
    event.wait_us = us;
    return event;
}

FlashEvent FlashEvent::wait_ready()
{
    FlashEvent event;
    event.type = EventType::WaitReady;
    return event;
}

FlashEvent FlashEvent::read_id()
{
    FlashEvent event;
    event.type = EventType::ReadId;
    return event;
}

FlashEvent FlashEvent::read_status(std::uint8_t index)
{
    FlashEvent event;
    event.type = EventType::ReadStatus;
    event.index = index;
    return event;
}

FlashEvent FlashEvent::nor_read(std::uint64_t address, std::size_t length, IoMode mode)
{
    // NOR 读使用 byte address + length；mode 只影响总线耗时和 QE 检查。
    FlashEvent event;
    event.type = EventType::NorRead;
    event.address = address;
    event.length = length;
    event.mode = mode;
    return event;
}

FlashEvent FlashEvent::nor_program(std::uint64_t address,
                                   const std::vector<std::uint8_t>& data,
                                   IoMode mode)
{
    // payload 保存调用方给出的待编程数据。old&new 规则不在事件层处理。
    FlashEvent event;
    event.type = EventType::NorProgram;
    event.address = address;
    event.payload = data;
    event.mode = mode;
    return event;
}

FlashEvent FlashEvent::nor_erase(std::uint64_t address, EraseKind kind)
{
    FlashEvent event;
    event.type = EventType::NorErase;
    event.address = address;
    event.erase_kind = kind;
    return event;
}

FlashEvent FlashEvent::get_feature(std::uint8_t address)
{
    // index 字段在 feature/status 类事件中表示寄存器地址或状态寄存器编号。
    FlashEvent event;
    event.type = EventType::GetFeature;
    event.index = address;
    return event;
}

FlashEvent FlashEvent::set_feature(std::uint8_t address, std::uint8_t value)
{
    FlashEvent event;
    event.type = EventType::SetFeature;
    event.index = address;
    event.value = value;
    return event;
}

FlashEvent FlashEvent::page_read(std::uint32_t page)
{
    // SPI-NAND row address 在本模型中统一使用线性 page index。
    FlashEvent event;
    event.type = EventType::PageRead;
    event.address = page;
    return event;
}

FlashEvent FlashEvent::read_from_cache(std::uint16_t column, std::size_t length, IoMode mode)
{
    FlashEvent event;
    event.type = EventType::ReadFromCache;
    event.column = column;
    event.length = length;
    event.mode = mode;
    return event;
}

FlashEvent FlashEvent::program_load(std::uint16_t column,
                                    const std::vector<std::uint8_t>& data,
                                    bool random_load,
                                    IoMode mode)
{
    // column 是 cache register 内偏移；random_load=true 表示不清空已有 cache。
    FlashEvent event;
    event.type = EventType::ProgramLoad;
    event.column = column;
    event.payload = data;
    event.random_load = random_load;
    event.mode = mode;
    return event;
}

FlashEvent FlashEvent::program_execute(std::uint32_t page)
{
    FlashEvent event;
    event.type = EventType::ProgramExecute;
    event.address = page;
    return event;
}

FlashEvent FlashEvent::block_erase(std::uint32_t block)
{
    FlashEvent event;
    event.type = EventType::BlockErase;
    event.address = block;
    return event;
}

} // namespace flashmod
