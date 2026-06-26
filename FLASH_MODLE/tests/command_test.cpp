#include "flash_model/command.hpp"
#include "flash_model/loader.hpp"

#include <cassert>
#include <iostream>

using namespace flash_model;

namespace {

void test_default_nor_read_uses_current_address_width()
{
    ModelConfig config;
    TransactionConfig transaction = resolve_transaction(config, "nor_read");
    assert(transaction.has_opcode_value);
    assert(transaction.opcode_value == 0x03);
    assert(transaction.command_lanes == 1);
    assert(transaction.address_lanes == 1);
    assert(transaction.data_lanes == 1);

    CommandTransfer transfer = describe_transfer(config, "nor_read", 3, 0, 16);

    assert(transfer.request_bytes == 4);
    assert(transfer.response_bytes == 16);
    assert(transfer.turnaround);

    transfer = describe_transfer(config, "nor_read", 4, 0, 16);
    assert(transfer.request_bytes == 5);
    assert(transfer.response_bytes == 16);
}

void test_default_spinand_program_load()
{
    ModelConfig config;
    const CommandTransfer transfer = describe_transfer(config, "program_load", 3, 32, 0);

    assert(transfer.request_bytes == 35);
    assert(transfer.response_bytes == 0);
    assert(!transfer.turnaround);
}

void test_profile_override()
{
    ModelConfig config;
    TransactionConfig transaction;
    transaction.opcode_bytes = 2;
    transaction.address_bytes = 5;
    transaction.dummy_bytes = 1;
    transaction.dummy_cycles = 8;
    transaction.fixed_request_bytes = 4;
    transaction.fixed_response_bytes = 2;
    transaction.command_lanes = 1;
    transaction.address_lanes = 1;
    transaction.data_lanes = 4;
    transaction.use_current_address_bytes = false;
    transaction.write_payload = true;
    transaction.read_response = true;
    transaction.turnaround = true;
    config.transactions["nor_read"] = transaction;

    const CommandTransfer transfer = describe_transfer(config, "nor_read", 3, 8, 16);

    assert(transfer.request_bytes == 20);
    assert(transfer.response_bytes == 18);
    assert(transfer.turnaround);
}

void test_alias_transaction()
{
    ModelConfig config;
    TransactionConfig alias;
    alias.alias_of = "nor_read";
    alias.opcode_value = 0x0B;
    alias.has_opcode_value = true;
    config.transactions["fast_read"] = alias;

    const TransactionConfig resolved = resolve_transaction(config, "fast_read");
    assert(resolved.has_opcode_value);
    assert(resolved.opcode_value == 0x0B);
    assert(resolved.use_current_address_bytes);
    assert(resolved.read_response);

    const CommandTransfer transfer = describe_transfer(config, "fast_read", 4, 0, 32);
    assert(transfer.request_bytes == 5);
    assert(transfer.response_bytes == 32);
}

void test_loader_reads_transaction_section()
{
    const ModelConfig config = load_config_file("configs/demo_nor.yaml");
    const auto it = config.transactions.find("nor_read");
    assert(it != config.transactions.end());
    assert(it->second.has_opcode_value);
    assert(it->second.opcode_value == 0x03);
    assert(it->second.opcode_bytes == 1);
    assert(it->second.dummy_cycles == 0);
    assert(it->second.command_lanes == 1);
    assert(it->second.address_lanes == 1);
    assert(it->second.data_lanes == 1);
    assert(it->second.use_current_address_bytes);
    assert(it->second.read_response);

    const TransactionConfig fast_read = resolve_transaction(config, "fast_read");
    assert(fast_read.has_opcode_value);
    assert(fast_read.opcode_value == 0x0B);
    assert(fast_read.use_current_address_bytes);
}

} // namespace

int main()
{
    test_default_nor_read_uses_current_address_width();
    test_default_spinand_program_load();
    test_profile_override();
    test_alias_transaction();
    test_loader_reads_transaction_section();

    std::cout << "Command transaction tests PASS\n";
    return 0;
}
