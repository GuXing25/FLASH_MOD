#include "flash_model/builder.hpp"
#include "flash_model/loader.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

using namespace flash_model;

namespace {

bool all_equal(const std::vector<std::uint8_t>& data, std::uint8_t value)
{
    return std::all_of(data.begin(), data.end(),
                       [value](std::uint8_t byte) { return byte == value; });
}

std::uint16_t le16(const std::vector<std::uint8_t>& data, std::size_t offset)
{
    return static_cast<std::uint16_t>(data[offset] |
                                      (static_cast<std::uint16_t>(data[offset + 1]) << 8));
}

std::uint32_t le32(const std::vector<std::uint8_t>& data, std::size_t offset)
{
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

void require_ok(const OperationResult& result)
{
    assert(result.ok && result.message.c_str());
}

void require_fail(const OperationResult& result)
{
    assert(!result.ok && result.message.c_str());
}

void test_nor_suspend_and_security_lock()
{
    FlashModel nor = build_model_from_file("configs/demo_nor.yaml");

    require_ok(nor.write_enable());
    require_ok(nor.nor_sector_erase(0x120));
    assert(nor.busy());
    require_ok(nor.suspend());
    assert(nor.state().suspended);
    require_ok(nor.nor_read(0x120, 4));
    require_fail(nor.nor_program(0x120, {0xAA}));
    require_ok(nor.resume());
    assert(!nor.state().suspended);
    assert(nor.busy());
    require_ok(nor.wait_ready());

    require_ok(nor.write_enable());
    require_ok(nor.erase_security_register(0));
    require_ok(nor.wait_ready());
    require_ok(nor.write_enable());
    require_ok(nor.program_security_register(0, 0, {0x5A, 0x5A}));
    require_ok(nor.wait_ready());
    require_ok(nor.write_enable());
    require_ok(nor.lock_security_register(0));
    require_ok(nor.wait_ready());
    require_ok(nor.write_enable());
    require_fail(nor.program_security_register(0, 0, {0x00}));
}

void test_nor_sfdp_unique_id_and_block32()
{
    FlashModel nor = build_model_from_file("configs/demo_nor.yaml");

    OperationResult uid = nor.read_unique_id();
    require_ok(uid);
    assert(uid.data.size() == 8);
    assert(uid.data[0] == 0x01);

    OperationResult sfdp = nor.read_sfdp(0, 8);
    require_ok(sfdp);
    assert(sfdp.data.size() == 8);
    assert(sfdp.data[0] == 'S');
    assert(sfdp.data[1] == 'F');
    assert(sfdp.data[2] == 'D');
    assert(sfdp.data[3] == 'P');

    require_ok(nor.write_enable());
    require_ok(nor.nor_sector_erase(0));
    require_ok(nor.wait_ready());
    require_ok(nor.write_enable());
    require_ok(nor.nor_program(0x120, {0x00, 0x00, 0x00, 0x00}));
    require_ok(nor.wait_ready());
    OperationResult before = nor.nor_read(0x120, 4);
    require_ok(before);
    assert(all_equal(before.data, 0x00));

    require_ok(nor.write_enable());
    require_ok(nor.nor_block32_erase(0x1234));
    require_ok(nor.wait_ready());
    OperationResult after = nor.nor_read(0x120, 4);
    require_ok(after);
    assert(all_equal(after.data, 0xFF));
}

void test_macronix_four_byte_profile()
{
    FlashModel mx = build_model_from_file("configs/mx25l25645g.yaml");
    assert(mx.config().geometry.memory_size == 33554432);
    assert(mx.config().geometry.block32_size == 32768);
    assert(mx.state().address_bytes == 3);
    require_ok(mx.enter_4byte_address());
    assert(mx.state().address_bytes == 4);
    require_ok(mx.exit_4byte_address());
    assert(mx.state().address_bytes == 3);
}

void test_gigadevice_spinand_parameter_page()
{
    FlashModel gd = build_model_from_file("configs/gd5f1gm7ue.yaml");
    OperationResult uid = gd.read_unique_id();
    require_ok(uid);
    assert(uid.data.size() == 16);

    OperationResult param = gd.read_parameter_page(0, 16);
    require_ok(param);
    assert(param.data.size() == 16);
    assert(param.data[0] == 'O');
    assert(param.data[1] == 'N');
    assert(param.data[2] == 'F');
    assert(param.data[3] == 'I');

    OperationResult maker = gd.read_parameter_page(32, 10);
    require_ok(maker);
    assert(maker.data[0] == 'G');
}

void test_datasheet_nand_parameter_pages()
{
    FlashModel winbond = build_model_from_file("configs/w25n01gv.yaml");
    OperationResult w25_param = winbond.read_parameter_page(0, 512);
    require_ok(w25_param);
    assert(w25_param.data[0] == 'O');
    assert(w25_param.data[1] == 'N');
    assert(w25_param.data[2] == 'F');
    assert(w25_param.data[3] == 'I');
    assert(w25_param.data[256] == 'O');
    assert(le32(w25_param.data, 80) == 2048);
    assert(le16(w25_param.data, 84) == 64);
    assert(le32(w25_param.data, 92) == 64);
    assert(le32(w25_param.data, 96) == 1024);
    assert(le16(w25_param.data, 103) == 20);

    FlashModel micron = build_model_from_file("configs/mt29f2g01.yaml");
    OperationResult mt_param = micron.read_parameter_page(0, 512);
    require_ok(mt_param);
    assert(mt_param.data[0] == 'O');
    assert(mt_param.data[248] == 8);
    assert(mt_param.data[256] == 'O');
    assert(le32(mt_param.data, 80) == 2048);
    assert(le16(mt_param.data, 84) == 128);
    assert(le32(mt_param.data, 96) == 2048);
    assert(le16(mt_param.data, 103) == 40);
}

void test_additional_nor_profiles()
{
    FlashModel m25 = build_model_from_file("configs/m25p40.yaml");
    assert(m25.config().geometry.memory_size == 524288);
    assert(m25.config().geometry.sector_size == 65536);
    OperationResult m25_uid = m25.read_unique_id();
    require_ok(m25_uid);
    assert(m25_uid.data.size() == 16);

    FlashModel gd25 = build_model_from_file("configs/gd25le128e.yaml");
    assert(gd25.config().geometry.memory_size == 16777216);
    assert(gd25.config().geometry.block32_size == 32768);
    assert(gd25.config().constraints.security_register_size == 1024);
    OperationResult sfdp = gd25.read_sfdp(0, 4);
    require_ok(sfdp);
    assert(sfdp.data[0] == 'S');
    assert(sfdp.data[1] == 'F');
    assert(sfdp.data[2] == 'D');
    assert(sfdp.data[3] == 'P');
}

void test_spinand_otp_storage()
{
    FlashModel nand = build_model_from_file("configs/demo_spinand.yaml");

    require_ok(nand.set_feature(0xA0, 0x00));
    require_ok(nand.enter_otp_mode());
    require_fail(nand.block_erase(0));

    std::vector<std::uint8_t> otp_data(16, 0xA6);
    require_ok(nand.write_enable());
    require_ok(nand.program_load(0, otp_data));
    require_ok(nand.program_execute(0));
    require_ok(nand.wait_ready());
    require_ok(nand.page_read(0));
    require_ok(nand.wait_ready());
    OperationResult otp_read = nand.read_from_cache(0, otp_data.size());
    require_ok(otp_read);
    assert(all_equal(otp_read.data, 0xA6));

    require_ok(nand.exit_otp_mode());
    require_ok(nand.page_read(0));
    require_ok(nand.wait_ready());
    OperationResult main_read = nand.read_from_cache(0, otp_data.size());
    require_ok(main_read);
    assert(all_equal(main_read.data, 0xFF));
}

void test_selection_and_read_retry()
{
    FlashModel micron = build_model_from_file("configs/mt29f2g01.yaml");
    require_ok(micron.select_plane(1));
    assert(micron.state().active_plane == 1);
    require_fail(micron.select_plane(2));
    require_ok(micron.select_die(0));
    require_fail(micron.select_die(1));

    ModelConfig retry_config = load_config_file("configs/demo_spinand.yaml");
    retry_config.capabilities.read_retry = true;
    retry_config.constraints.read_retry_levels = 3;
    FlashModel retry = build_model(std::move(retry_config));
    require_ok(retry.set_read_retry_level(2));
    assert(retry.state().read_retry_level == 2);
    require_fail(retry.set_read_retry_level(3));
}

} // namespace

int main()
{
    test_nor_suspend_and_security_lock();
    test_nor_sfdp_unique_id_and_block32();
    test_macronix_four_byte_profile();
    test_gigadevice_spinand_parameter_page();
    test_datasheet_nand_parameter_pages();
    test_additional_nor_profiles();
    test_spinand_otp_storage();
    test_selection_and_read_retry();

    std::cout << "Model feature tests PASS\n";
    return 0;
}
