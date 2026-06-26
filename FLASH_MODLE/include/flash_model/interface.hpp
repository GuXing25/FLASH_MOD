#ifndef FLASH_MODEL_INTERFACE_HPP
#define FLASH_MODEL_INTERFACE_HPP

#include "flash_model/config.hpp"

#include <cstdint>

namespace flash_model {

struct InterfaceTransaction {
    std::uint64_t request_bytes = 0; // 请求方向传输字节数。
    std::uint64_t response_bytes = 0; // 响应方向传输字节数。
    bool turnaround = true; // 是否计入方向切换耗时。
};

// InterfaceEngine 是 host/controller 与 flash 模型之间的行为级接口计时器。
// 它只把逻辑 transaction 转成耗时，不建模引脚、电气或 cycle-accurate PHY。
class InterfaceEngine {
public:
    explicit InterfaceEngine(const ModelConfig& config);

    bool enabled() const;
    // 从结构化 transaction 计算耗时。
    double transfer_time_us(const InterfaceTransaction& transaction) const;
    // 从请求/响应字节数直接计算耗时。
    double transfer_time_us(std::uint64_t request_bytes,
                            std::uint64_t response_bytes,
                            bool turnaround = true) const;

private:
    InterfaceConfig config_; // 接口参数快照。

    double bytes_per_us() const;
    std::uint64_t packet_count(std::uint64_t payload_bytes) const;
};

} // namespace flash_model

#endif
