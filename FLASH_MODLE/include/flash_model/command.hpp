#ifndef FLASH_MODEL_COMMAND_HPP
#define FLASH_MODEL_COMMAND_HPP

#include "flash_model/config.hpp"

#include <cstdint>
#include <string>

namespace flash_model {

struct CommandTransfer {
    std::uint64_t request_bytes = 0;
    std::uint64_t response_bytes = 0;
    bool turnaround = true;
};

// Resolves the transaction shape of a command. The result is intentionally
// behavior-level: it describes command/address/dummy/payload/response bytes for
// interface timing, not opcode semantics or pin-level protocol phases.
TransactionConfig resolve_transaction(const ModelConfig& config,
                                      const std::string& command_name);

CommandTransfer describe_transfer(const TransactionConfig& transaction,
                                  std::uint32_t current_address_bytes,
                                  std::uint64_t write_payload_bytes,
                                  std::uint64_t read_response_bytes);

CommandTransfer describe_transfer(const ModelConfig& config,
                                  const std::string& command_name,
                                  std::uint32_t current_address_bytes,
                                  std::uint64_t write_payload_bytes = 0,
                                  std::uint64_t read_response_bytes = 0);

bool is_known_transaction(const std::string& command_name);

} // namespace flash_model

#endif
