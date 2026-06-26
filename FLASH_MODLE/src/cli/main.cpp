#include "flash_model/builder.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace flash_model;

namespace {

void print_hex(const std::string& label, const std::vector<std::uint8_t>& data, std::size_t limit = 16)
{
    std::cout << label;
    const std::size_t n = std::min(limit, data.size());
    for (std::size_t i = 0; i < n; ++i) {
        std::cout << " " << std::hex << std::uppercase << std::setw(2)
                  << std::setfill('0') << static_cast<int>(data[i]);
    }
    std::cout << std::dec << std::nouppercase << std::setfill(' ');
    if (data.size() > n) std::cout << " ...";
    std::cout << "\n";
}

void require_ok(const OperationResult& result)
{
    std::cout << "[cmd] " << result.message
              << " ok=" << (result.ok ? 1 : 0)
              << " now=" << std::fixed << std::setprecision(3)
              << result.now_us << "us\n";
    if (!result.ok) throw std::runtime_error(result.message);
}

void expect_fail(const OperationResult& result, const std::string& label)
{
    std::cout << "[cmd] " << result.message
              << " ok=" << (result.ok ? 1 : 0)
              << " expected_fail=" << label
              << " now=" << std::fixed << std::setprecision(3)
              << result.now_us << "us\n";
    if (result.ok) throw std::runtime_error(label + " unexpectedly succeeded");
}

bool all_equal(const std::vector<std::uint8_t>& data, std::uint8_t value)
{
    return std::all_of(data.begin(), data.end(),
                       [value](std::uint8_t byte) { return byte == value; });
}

bool run_nor_self_test(FlashModel& model)
{
    const ModelConfig& config = model.config();
    std::uint64_t address = 0x120;
    OperationResult id = model.read_id();
    require_ok(id);
    print_hex("[data] ID", id.data);
    bool ok = true;

    if (config.capabilities.unique_id && config.commands.read_unique_id) {
        OperationResult uid = model.read_unique_id();
        require_ok(uid);
        print_hex("[data] Unique ID", uid.data);
        ok = ok && uid.data.size() == config.constraints.unique_id_size;
    }

    if (config.capabilities.sfdp && config.commands.read_sfdp) {
        OperationResult sfdp = model.read_sfdp(0, 8);
        require_ok(sfdp);
        print_hex("[data] SFDP", sfdp.data);
        ok = ok && sfdp.data.size() >= 4 &&
             sfdp.data[0] == 'S' && sfdp.data[1] == 'F' &&
             sfdp.data[2] == 'D' && sfdp.data[3] == 'P';
    }

    if (config.capabilities.four_byte_address &&
        config.commands.enter_4byte_address &&
        config.commands.exit_4byte_address) {
        require_ok(model.enter_4byte_address());
        ok = ok && model.state().address_bytes == 4;
        require_ok(model.exit_4byte_address());
        ok = ok && model.state().address_bytes == config.constraints.address_bytes;
    }

    if (config.capabilities.block_protect && config.constraints.nor_protect_length > 0) {
        const std::uint64_t protected_address = config.constraints.nor_protect_start;
        std::vector<std::uint8_t> probe(16, 0x55);
        require_ok(model.write_enable());
        expect_fail(model.nor_sector_erase(protected_address),
                    "NOR block protect rejects erase in protected range");
        require_ok(model.write_enable());
        expect_fail(model.nor_program(protected_address, probe),
                    "NOR block protect rejects program in protected range");

        address = config.constraints.nor_protect_start +
                  config.constraints.nor_protect_length +
                  config.geometry.page_size;
        if (address + 64 >= config.geometry.memory_size) {
            throw std::runtime_error("NOR block_protect self-test has no unprotected test address");
        }
    }

    require_ok(model.write_enable());
    require_ok(model.nor_sector_erase(address));
    if (config.capabilities.suspend_resume && config.commands.suspend && config.commands.resume) {
        require_ok(model.suspend());
        require_ok(model.nor_read(address, 1));
        require_ok(model.resume());
    }
    require_ok(model.wait_ready());

    std::vector<std::uint8_t> data(64, 0xAA);
    require_ok(model.write_enable());
    require_ok(model.nor_program(address, data));
    require_ok(model.wait_ready());

    OperationResult read1 = model.nor_read(address, data.size());
    require_ok(read1);
    print_hex("[data] NOR read", read1.data);
    ok = ok && all_equal(read1.data, 0xAA);

    std::vector<std::uint8_t> patch(16, 0xF0);
    require_ok(model.write_enable());
    require_ok(model.nor_program(address, patch));
    require_ok(model.wait_ready());

    OperationResult read2 = model.nor_read(address, patch.size());
    require_ok(read2);
    print_hex("[data] NOR old&new", read2.data);
    ok = ok && all_equal(read2.data, 0xA0);

    if (config.geometry.page_size > 8) {
        const std::uint64_t wrap_base = (address / config.geometry.page_size) * config.geometry.page_size;
        const std::uint64_t wrap_address = wrap_base + config.geometry.page_size - 4;
        std::vector<std::uint8_t> wrap_data(8, 0x3C);

        require_ok(model.write_enable());
        require_ok(model.nor_sector_erase(wrap_base));
        require_ok(model.wait_ready());
        require_ok(model.write_enable());
        require_ok(model.nor_program(wrap_address, wrap_data));
        require_ok(model.wait_ready());

        OperationResult tail = model.nor_read(wrap_address, 4);
        require_ok(tail);
        OperationResult head = model.nor_read(wrap_base, 4);
        require_ok(head);
        print_hex("[data] NOR page wrap tail", tail.data);
        print_hex("[data] NOR page wrap head", head.data);
        ok = ok && all_equal(tail.data, 0x3C) && all_equal(head.data, 0x3C);
    }

    if (config.commands.nor_block32_erase) {
        require_ok(model.write_enable());
        require_ok(model.nor_block32_erase(address));
        require_ok(model.wait_ready());

        OperationResult block32_read = model.nor_read(address, patch.size());
        require_ok(block32_read);
        print_hex("[data] NOR block32 erase", block32_read.data);
        ok = ok && all_equal(block32_read.data, 0xFF);
    }

    if (config.commands.nor_block_erase) {
        require_ok(model.write_enable());
        require_ok(model.nor_block_erase(address));
        require_ok(model.wait_ready());

        OperationResult block_read = model.nor_read(address, patch.size());
        require_ok(block_read);
        print_hex("[data] NOR block erase", block_read.data);
        ok = ok && all_equal(block_read.data, 0xFF);
    }

    if (config.commands.nor_chip_erase && config.constraints.nor_protect_length == 0) {
        require_ok(model.write_enable());
        require_ok(model.nor_chip_erase());
        require_ok(model.wait_ready());

        OperationResult chip_read = model.nor_read(address, patch.size());
        require_ok(chip_read);
        print_hex("[data] NOR chip erase", chip_read.data);
        ok = ok && all_equal(chip_read.data, 0xFF);
    }

    if (config.capabilities.security_register &&
        config.commands.read_security_register &&
        config.commands.program_security_register &&
        config.commands.erase_security_register) {
        std::vector<std::uint8_t> sec_data(16, 0x5A);
        std::vector<std::uint8_t> sec_patch(16, 0xF0);

        require_ok(model.write_enable());
        require_ok(model.erase_security_register(0));
        require_ok(model.wait_ready());
        require_ok(model.write_enable());
        require_ok(model.program_security_register(0, 0, sec_data));
        require_ok(model.wait_ready());

        OperationResult sec_read = model.read_security_register(0, 0, sec_data.size());
        require_ok(sec_read);
        print_hex("[data] NOR security read", sec_read.data);
        ok = ok && all_equal(sec_read.data, 0x5A);

        require_ok(model.write_enable());
        require_ok(model.program_security_register(0, 0, sec_patch));
        require_ok(model.wait_ready());

        OperationResult sec_old_new = model.read_security_register(0, 0, sec_patch.size());
        require_ok(sec_old_new);
        print_hex("[data] NOR security old&new", sec_old_new.data);
        ok = ok && all_equal(sec_old_new.data, 0x50);

        if (config.commands.lock_security_register) {
            require_ok(model.write_enable());
            require_ok(model.lock_security_register(0));
            require_ok(model.wait_ready());
            require_ok(model.write_enable());
            expect_fail(model.program_security_register(0, 0, {0x00}),
                        "NOR security register lock rejects program");
        }
    }

    if (config.capabilities.deep_power_down && config.commands.deep_power_down &&
        config.commands.release_power_down) {
        require_ok(model.deep_power_down());
        expect_fail(model.nor_read(address, 1), "deep power-down rejects normal read");
        require_ok(model.release_power_down());
        require_ok(model.nor_read(address, 1));
    }

    return ok;
}

bool run_spinand_self_test(FlashModel& model)
{
    const ModelConfig& config = model.config();
    OperationResult id = model.read_id();
    require_ok(id);
    print_hex("[data] ID", id.data);
    bool ok = true;

    if (config.capabilities.unique_id && config.commands.read_unique_id) {
        OperationResult uid = model.read_unique_id();
        require_ok(uid);
        print_hex("[data] NAND Unique ID", uid.data);
        ok = ok && uid.data.size() == config.constraints.unique_id_size;
    }

    if (config.capabilities.parameter_page && config.commands.read_parameter_page) {
        OperationResult param = model.read_parameter_page(0, 16);
        require_ok(param);
        print_hex("[data] NAND parameter page", param.data);
        ok = ok && param.data.size() >= 4 &&
             param.data[0] == 'O' && param.data[1] == 'N' &&
             param.data[2] == 'F' && param.data[3] == 'I';
    }

    OperationResult a0 = model.get_feature(0xA0);
    require_ok(a0);
    if (!a0.data.empty() && (a0.data[0] & 0x7Cu) != 0) {
        if (config.capabilities.block_protect) {
            require_ok(model.write_enable());
            expect_fail(model.block_erase(0), "block protect rejects erase before unlock");
        }
        require_ok(model.set_feature(0xA0, 0x00));
    }

    if (config.capabilities.otp && config.commands.enter_otp_mode &&
        config.commands.exit_otp_mode && config.constraints.otp_page_count > 0) {
        std::vector<std::uint8_t> otp_data(16, 0xA6);
        require_ok(model.enter_otp_mode());
        expect_fail(model.block_erase(0), "NAND OTP mode rejects block erase");
        require_ok(model.write_enable());
        require_ok(model.program_load(0, otp_data));
        require_ok(model.program_execute(0));
        require_ok(model.wait_ready());
        require_ok(model.page_read(0));
        require_ok(model.wait_ready());
        OperationResult otp_read = model.read_from_cache(0, otp_data.size());
        require_ok(otp_read);
        print_hex("[data] NAND OTP read", otp_read.data);
        ok = ok && all_equal(otp_read.data, 0xA6);
        require_ok(model.exit_otp_mode());
    }

    require_ok(model.write_enable());
    require_ok(model.block_erase(0));
    require_ok(model.wait_ready());

    std::vector<std::uint8_t> data(128, 0xA5);
    require_ok(model.write_enable());
    require_ok(model.program_load(0, data));
    require_ok(model.program_execute(0));
    require_ok(model.wait_ready());

    require_ok(model.page_read(0));
    require_ok(model.wait_ready());
    OperationResult read1 = model.read_from_cache(0, data.size());
    require_ok(read1);
    print_hex("[data] NAND cache read", read1.data);
    ok = ok && all_equal(read1.data, 0xA5);

    std::vector<std::uint8_t> patch(16, 0xF0);
    require_ok(model.write_enable());
    require_ok(model.program_load(0, patch, true));
    require_ok(model.program_execute(0));
    require_ok(model.wait_ready());

    require_ok(model.page_read(0));
    require_ok(model.wait_ready());
    OperationResult read2 = model.read_from_cache(0, patch.size());
    require_ok(read2);
    print_hex("[data] NAND partial old&new", read2.data);
    ok = ok && all_equal(read2.data, 0xA0);

    if (config.capabilities.ecc_status && config.constraints.ecc_reserved_end > 0) {
        std::vector<std::uint8_t> ecc_probe(4, 0x00);
        require_ok(model.write_enable());
        require_ok(model.program_load(static_cast<std::uint16_t>(config.constraints.ecc_reserved_start),
                                      ecc_probe));
        expect_fail(model.program_execute(1), "ECC reserved spare range rejects program");
    }

    if (config.constraints.strict_sequential_program && config.geometry.pages_per_block > 2) {
        std::vector<std::uint8_t> seq_probe(8, 0x33);
        require_ok(model.write_enable());
        require_ok(model.program_load(0, seq_probe));
        expect_fail(model.program_execute(2), "strict sequential program rejects skipped page");
    }

    if (config.capabilities.copy_back && config.commands.copy_back) {
        require_ok(model.write_enable());
        require_ok(model.copy_back(0, 1));
        require_ok(model.wait_ready());

        require_ok(model.page_read(1));
        require_ok(model.wait_ready());
        OperationResult copy_read = model.read_from_cache(0, patch.size());
        require_ok(copy_read);
        print_hex("[data] NAND copy-back", copy_read.data);
        ok = ok && all_equal(copy_read.data, 0xA0);
    }

    if (config.capabilities.bad_block_management &&
        !config.constraints.initial_bad_blocks.empty()) {
        const std::uint32_t bad_block = config.constraints.initial_bad_blocks.front();
        const std::uint32_t bad_page = bad_block * config.geometry.pages_per_block;
        std::vector<std::uint8_t> bad_probe(8, 0x5A);

        require_ok(model.write_enable());
        expect_fail(model.block_erase(bad_block), "bad block management rejects erase");

        require_ok(model.write_enable());
        require_ok(model.program_load(0, bad_probe));
        expect_fail(model.program_execute(bad_page), "bad block management rejects program");
    }

    return ok;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        std::cout << std::unitbuf;

        std::string config_path = "configs/demo_nor.yaml";
        bool self_test = true;
        bool validate_only = false;

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--no-test") self_test = false;
            else if (arg == "--self-test") self_test = true;
            else if (arg == "--validate-only") {
                validate_only = true;
                self_test = false;
            }
            else config_path = arg;
        }

        FlashModel model = build_model_from_file(config_path);
        const ModelConfig& config = model.config();

        std::cout << "=== FLASH_MODEL Profile ===\n";
        std::cout << "name: " << config.device.name << "\n";
        std::cout << "class: " << to_string(config.device.cls) << "\n";
        std::cout << "policy: " << config.policy.name << "\n";
        std::cout << "total_size: " << config.total_size_bytes() << " bytes\n";
        std::cout << "page_size: " << config.effective_page_size() << "\n";
        std::cout << "capabilities:";
        for (const auto& cap : model.state().enabled_capabilities) std::cout << " " << cap;
        std::cout << "\n";

        if (validate_only) {
            std::cout << "Validation Result: PASS\n";
            return 0;
        }

        bool ok = true;
        if (self_test) {
            if (is_nand_like(config.device.cls)) ok = run_spinand_self_test(model);
            else ok = run_nor_self_test(model);
        }

        std::cout << "Test Result: " << (ok ? "PASS" : "FAIL") << "\n";
        std::cout << "Final Time: " << std::fixed << std::setprecision(3)
                  << model.time_us() << " us\n";
        return ok ? 0 : 2;
    } catch (const std::exception& e) {
        std::cerr << "[fatal] " << e.what() << "\n";
        return 1;
    }
}
