#include "ftl.h"

namespace flashmod {

FTL::FTL(FlashCore& core) : core_(core) {}

void FTL::submit(const FlashEvent& event)
{
    // 事件先入队，不立即执行；这给后续异步调度、合并请求或多 die 调度留入口。
    queue_.push(event);
}

std::vector<OperationResult> FTL::run()
{
    // 当前实现是简单 FIFO。只要 execute() 保持单事件语义，未来可以替换这里的调度策略。
    std::vector<OperationResult> results;
    while (!queue_.empty()) {
        results.push_back(execute(queue_.front()));
        queue_.pop();
    }
    return results;
}

OperationResult FTL::execute(const FlashEvent& event)
{
    // FTL 只做事件类型分发，不直接修改寄存器或 storage；所有器件语义都在 FlashCore。
    switch (event.type) {
    case EventType::WriteEnable:
        return core_.write_enable();
    case EventType::WriteDisable:
        return core_.write_disable();
    case EventType::Reset:
        return core_.reset();
    case EventType::WaitUs:
        return core_.wait_us(event.wait_us);
    case EventType::WaitReady:
        return core_.wait_ready();
    case EventType::ReadId:
        return core_.read_id();
    case EventType::ReadStatus: {
        // status 查询是轻量读，不需要 FlashCore 构造专门 OperationResult。
        OperationResult result;
        result.ok = true;
        result.message = "READ_STATUS";
        result.data.push_back(core_.status(event.index));
        result.now_us = core_.time_us();
        return result;
    }
    case EventType::NorRead:
        return core_.read(event.address, event.length, event.mode);
    case EventType::NorProgram:
        return core_.program(event.address, event.payload, event.mode);
    case EventType::NorErase:
        return core_.erase(event.address, event.erase_kind);
    case EventType::GetFeature:
        return core_.get_feature(event.index);
    case EventType::SetFeature:
        return core_.set_feature(event.index, event.value);
    case EventType::PageRead:
        return core_.page_read(static_cast<std::uint32_t>(event.address));
    case EventType::ReadFromCache:
        return core_.read_from_cache(event.column, event.length, event.mode);
    case EventType::ProgramLoad:
        return core_.program_load(event.column, event.payload, event.random_load, event.mode);
    case EventType::ProgramExecute:
        return core_.program_execute(static_cast<std::uint32_t>(event.address));
    case EventType::BlockErase:
        return core_.block_erase(static_cast<std::uint32_t>(event.address));
    }

    OperationResult result;
    // 正常情况下 switch 已覆盖所有 EventType；这里是防御性兜底，避免新增事件后静默成功。
    result.ok = false;
    result.message = "UNKNOWN_EVENT";
    result.now_us = core_.time_us();
    return result;
}

OperationResult FTL::write_enable() { return execute(FlashEvent::write_enable()); }
OperationResult FTL::write_disable() { return execute(FlashEvent::write_disable()); }
OperationResult FTL::reset() { return execute(FlashEvent::reset()); }
OperationResult FTL::wait_us(double us) { return execute(FlashEvent::wait_us_cmd(us)); }
OperationResult FTL::wait_ready() { return execute(FlashEvent::wait_ready()); }
OperationResult FTL::read_id() { return execute(FlashEvent::read_id()); }
OperationResult FTL::read_status(std::uint8_t index) { return execute(FlashEvent::read_status(index)); }

OperationResult FTL::read(std::uint64_t address, std::size_t length, IoMode mode)
{
    // 便捷 API 只是事件工厂的薄封装。上层也可以直接 submit(FlashEvent::nor_read(...))。
    return execute(FlashEvent::nor_read(address, length, mode));
}

OperationResult FTL::program(std::uint64_t address,
                             const std::vector<std::uint8_t>& data,
                             IoMode mode)
{
    return execute(FlashEvent::nor_program(address, data, mode));
}

OperationResult FTL::erase(std::uint64_t address, EraseKind kind)
{
    return execute(FlashEvent::nor_erase(address, kind));
}

OperationResult FTL::get_feature(std::uint8_t address)
{
    return execute(FlashEvent::get_feature(address));
}

OperationResult FTL::set_feature(std::uint8_t address, std::uint8_t value)
{
    return execute(FlashEvent::set_feature(address, value));
}

OperationResult FTL::page_read(std::uint32_t page)
{
    return execute(FlashEvent::page_read(page));
}

OperationResult FTL::read_from_cache(std::uint16_t column, std::size_t length, IoMode mode)
{
    return execute(FlashEvent::read_from_cache(column, length, mode));
}

OperationResult FTL::program_load(std::uint16_t column,
                                  const std::vector<std::uint8_t>& data,
                                  bool random_load,
                                  IoMode mode)
{
    return execute(FlashEvent::program_load(column, data, random_load, mode));
}

OperationResult FTL::program_execute(std::uint32_t page)
{
    return execute(FlashEvent::program_execute(page));
}

OperationResult FTL::block_erase(std::uint32_t block)
{
    return execute(FlashEvent::block_erase(block));
}

} // namespace flashmod
