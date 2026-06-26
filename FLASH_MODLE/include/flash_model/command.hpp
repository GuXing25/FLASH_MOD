#ifndef FLASH_MODEL_COMMAND_HPP
#define FLASH_MODEL_COMMAND_HPP

#include "flash_model/config.hpp"

#include <cstdint>
#include <string>

namespace flash_model {

struct CommandTransfer {
    std::uint64_t request_bytes = 0; // host/controller 发向模型侧的字节数。
    std::uint64_t response_bytes = 0; // 模型侧返回 host/controller 的字节数。
    bool turnaround = true; // 是否需要计入写转读 turnaround。
};

// 解析命令事务形状。结果只用于行为级接口耗时，不负责真实 pin-level 协议。
TransactionConfig resolve_transaction(const ModelConfig& config,
                                      const std::string& command_name);

// 把事务形状和本次 payload/response 长度换算成传输字节数。
CommandTransfer describe_transfer(const TransactionConfig& transaction,
                                  std::uint32_t current_address_bytes,
                                  std::uint64_t write_payload_bytes,
                                  std::uint64_t read_response_bytes);

// 便捷入口：先按 command_name 解析事务，再计算传输字节数。
CommandTransfer describe_transfer(const ModelConfig& config,
                                  const std::string& command_name,
                                  std::uint32_t current_address_bytes,
                                  std::uint64_t write_payload_bytes = 0,
                                  std::uint64_t read_response_bytes = 0);

// validator 用它区分“覆盖已知命令”和“新增未知命令”。
bool is_known_transaction(const std::string& command_name);

} // namespace flash_model

#endif
