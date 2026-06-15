#ifndef FLASH_MOD_FTL_H
#define FLASH_MOD_FTL_H

#include "flash_core.h"
#include "flash_event.h"

#include <cstdint>
#include <queue>
#include <vector>

namespace flashmod {

// FTL 是当前模型的上层外观层：它负责接收 FlashEvent、做简单 FIFO 调度，
// 再把命令分发给 FlashCore。后续如果加入 LBA->PBA、多 plane 调度、坏块替换，
// 也应优先扩展这一层，而不是把策略塞进 FlashCore。
class FTL {
public:
    explicit FTL(FlashCore& core);

    // 透传只读状态，方便测试或上层调度器观察器件状态。
    const DeviceProfile& profile() const { return core_.profile(); }
    double time_us() const { return core_.time_us(); }
    bool is_busy() { return core_.is_busy(); }
    std::uint8_t status(std::uint8_t index) { return core_.status(index); }

    // 异步风格入口：submit() 入队，run() 顺序执行当前队列并返回每条命令结果。
    void submit(const FlashEvent& event);
    std::vector<OperationResult> run();

    // 同步执行一条事件。下面所有便捷 API 最终都会构造 FlashEvent 并走 execute()。
    OperationResult execute(const FlashEvent& event);

    // 通用控制命令。
    OperationResult write_enable();
    OperationResult write_disable();
    OperationResult reset();
    OperationResult wait_us(double us);
    OperationResult wait_ready();
    OperationResult read_id();
    OperationResult read_status(std::uint8_t index);

    // NOR direct-array API。
    OperationResult read(std::uint64_t address, std::size_t length, IoMode mode = IoMode::X1);
    OperationResult program(std::uint64_t address,
                            const std::vector<std::uint8_t>& data,
                            IoMode mode = IoMode::X1);
    OperationResult erase(std::uint64_t address, EraseKind kind);

    // SPI-NAND cache/page API。
    OperationResult get_feature(std::uint8_t address);
    OperationResult set_feature(std::uint8_t address, std::uint8_t value);
    OperationResult page_read(std::uint32_t page);
    OperationResult read_from_cache(std::uint16_t column,
                                    std::size_t length,
                                    IoMode mode = IoMode::X1);
    OperationResult program_load(std::uint16_t column,
                                 const std::vector<std::uint8_t>& data,
                                 bool random_load = false,
                                 IoMode mode = IoMode::X1);
    OperationResult program_execute(std::uint32_t page);
    OperationResult block_erase(std::uint32_t block);

private:
    // FTL 不拥有 FlashCore；main 或更上层框架负责保证 core 生命周期。
    FlashCore& core_;
    std::queue<FlashEvent> queue_;
};

} // namespace flashmod

#endif
