#include "flash_model/command.hpp"

#include <map>

namespace flash_model {
namespace {

TransactionConfig tx(std::uint32_t opcode_bytes,
                     std::uint32_t address_bytes,
                     std::uint32_t dummy_bytes,
                     std::uint32_t fixed_request_bytes,
                     std::uint32_t fixed_response_bytes,
                     bool use_current_address_bytes,
                     bool write_payload,
                     bool read_response,
                     bool turnaround)
{
    TransactionConfig transaction;
    transaction.opcode_bytes = opcode_bytes;
    transaction.address_bytes = address_bytes;
    transaction.dummy_bytes = dummy_bytes;
    transaction.fixed_request_bytes = fixed_request_bytes;
    transaction.fixed_response_bytes = fixed_response_bytes;
    transaction.use_current_address_bytes = use_current_address_bytes;
    transaction.write_payload = write_payload;
    transaction.read_response = read_response;
    transaction.turnaround = turnaround;
    return transaction;
}

TransactionConfig with_opcode(TransactionConfig transaction, std::uint32_t opcode)
{
    transaction.opcode_value = opcode;
    transaction.has_opcode_value = true;
    return transaction;
}

const std::map<std::string, TransactionConfig>& default_transactions()
{
    static const std::map<std::string, TransactionConfig> table = {
        {"read_id", with_opcode(tx(1, 0, 0, 0, 0, false, false, true, true), 0x9F)},
        {"write_enable", with_opcode(tx(1, 0, 0, 0, 0, false, false, false, false), 0x06)},
        {"write_disable", with_opcode(tx(1, 0, 0, 0, 0, false, false, false, false), 0x04)},
        {"reset", with_opcode(tx(1, 0, 0, 0, 0, false, false, false, false), 0xFF)},
        {"deep_power_down", with_opcode(tx(1, 0, 0, 0, 0, false, false, false, false), 0xB9)},
        {"release_power_down", with_opcode(tx(1, 0, 0, 0, 0, false, false, false, false), 0xAB)},
        {"enter_4byte_address", with_opcode(tx(1, 0, 0, 0, 0, false, false, false, false), 0xB7)},
        {"exit_4byte_address", with_opcode(tx(1, 0, 0, 0, 0, false, false, false, false), 0xE9)},
        {"suspend", with_opcode(tx(1, 0, 0, 0, 0, false, false, false, false), 0x75)},
        {"resume", with_opcode(tx(1, 0, 0, 0, 0, false, false, false, false), 0x7A)},
        {"read_unique_id", with_opcode(tx(1, 0, 4, 0, 0, false, false, true, true), 0x4B)},
        {"read_sfdp", with_opcode(tx(1, 3, 1, 0, 0, false, false, true, true), 0x5A)},

        {"nor_read", with_opcode(tx(1, 0, 0, 0, 0, true, false, true, true), 0x03)},
        {"nor_program", with_opcode(tx(1, 0, 0, 0, 0, true, true, false, false), 0x02)},
        {"nor_sector_erase", with_opcode(tx(1, 0, 0, 0, 0, true, false, false, false), 0x20)},
        {"nor_block32_erase", with_opcode(tx(1, 0, 0, 0, 0, true, false, false, false), 0x52)},
        {"nor_block_erase", with_opcode(tx(1, 0, 0, 0, 0, true, false, false, false), 0xD8)},
        {"nor_chip_erase", with_opcode(tx(1, 0, 0, 0, 0, false, false, false, false), 0xC7)},
        {"security_read", with_opcode(tx(1, 0, 1, 0, 0, true, false, true, true), 0x48)},
        {"security_erase", with_opcode(tx(1, 0, 0, 0, 0, true, false, false, false), 0x44)},
        {"security_program", with_opcode(tx(1, 0, 0, 0, 0, true, true, false, false), 0x42)},
        {"security_lock", with_opcode(tx(1, 0, 0, 0, 0, true, false, false, false), 0x36)},

        {"enter_otp_mode", with_opcode(tx(1, 0, 0, 0, 0, false, false, false, false), 0x1F)},
        {"exit_otp_mode", with_opcode(tx(1, 0, 0, 0, 0, false, false, false, false), 0x1F)},
        {"get_feature", with_opcode(tx(1, 0, 0, 1, 1, false, false, false, true), 0x0F)},
        {"set_feature", with_opcode(tx(1, 0, 0, 2, 0, false, false, false, false), 0x1F)},
        {"select_die", with_opcode(tx(1, 0, 0, 1, 0, false, false, false, false), 0xC2)},
        {"select_plane", tx(1, 0, 0, 1, 0, false, false, false, false)},
        {"read_retry", tx(1, 0, 0, 1, 0, false, false, false, false)},
        {"block_erase", with_opcode(tx(1, 3, 0, 0, 0, false, false, false, false), 0xD8)},
        {"program_load", with_opcode(tx(1, 2, 0, 0, 0, false, true, false, false), 0x02)},
        {"program_execute", with_opcode(tx(1, 3, 0, 0, 0, false, false, false, false), 0x10)},
        {"copy_back", tx(0, 0, 0, 8, 0, false, false, false, false)},
        {"page_read", with_opcode(tx(1, 3, 0, 0, 0, false, false, false, false), 0x13)},
        {"read_from_cache", with_opcode(tx(1, 2, 1, 0, 0, false, false, true, true), 0x03)},
        {"read_parameter_page", with_opcode(tx(1, 2, 1, 0, 0, false, false, true, true), 0xEC)},
    };
    return table;
}

} // namespace

TransactionConfig resolve_transaction(const ModelConfig& config,
                                      const std::string& command_name)
{
    const auto override_it = config.transactions.find(command_name);
    if (override_it != config.transactions.end()) {
        const TransactionConfig& override_tx = override_it->second;
        if (!override_tx.alias_of.empty() && override_tx.alias_of != command_name) {
            TransactionConfig resolved = resolve_transaction(config, override_tx.alias_of);
            if (override_tx.has_opcode_value) {
                resolved.opcode_value = override_tx.opcode_value;
                resolved.has_opcode_value = true;
            }
            return resolved;
        }
        return override_tx;
    }

    const auto default_it = default_transactions().find(command_name);
    if (default_it != default_transactions().end()) return default_it->second;

    return TransactionConfig{};
}

CommandTransfer describe_transfer(const TransactionConfig& transaction,
                                  std::uint32_t current_address_bytes,
                                  std::uint64_t write_payload_bytes,
                                  std::uint64_t read_response_bytes)
{
    const std::uint64_t address_bytes = transaction.use_current_address_bytes
                                            ? current_address_bytes
                                            : transaction.address_bytes;

    CommandTransfer transfer;
    transfer.request_bytes = transaction.opcode_bytes +
                             address_bytes +
                             transaction.dummy_bytes +
                             transaction.fixed_request_bytes;
    transfer.response_bytes = transaction.fixed_response_bytes;
    transfer.turnaround = transaction.turnaround;

    if (transaction.write_payload) transfer.request_bytes += write_payload_bytes;
    if (transaction.read_response) transfer.response_bytes += read_response_bytes;
    return transfer;
}

CommandTransfer describe_transfer(const ModelConfig& config,
                                  const std::string& command_name,
                                  std::uint32_t current_address_bytes,
                                  std::uint64_t write_payload_bytes,
                                  std::uint64_t read_response_bytes)
{
    return describe_transfer(resolve_transaction(config, command_name),
                             current_address_bytes,
                             write_payload_bytes,
                             read_response_bytes);
}

bool is_known_transaction(const std::string& command_name)
{
    return default_transactions().find(command_name) != default_transactions().end();
}

} // namespace flash_model
