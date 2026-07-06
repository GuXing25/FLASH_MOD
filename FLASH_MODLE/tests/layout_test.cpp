#include "flash_model/address.hpp"
#include "flash_model/builder.hpp"
#include "flash_model/loader.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace flash_model;

namespace {

void check_profile(const std::string& path)
{
    ModelConfig config = load_config_file(path);
    FlashModel model = build_model(config);

    if (is_nand_like(config.device.cls)) {
        const std::uint64_t page_size =
            static_cast<std::uint64_t>(config.geometry.main_size) +
            static_cast<std::uint64_t>(config.geometry.spare_size);
        const std::uint64_t expected =
            static_cast<std::uint64_t>(config.geometry.blocks) *
            static_cast<std::uint64_t>(config.geometry.pages_per_block) *
            page_size;

        assert(config.effective_page_size() == page_size);
        assert(config.total_size_bytes() == expected);
        assert(model.storage_size_bytes() == expected);

        AddressMapper mapper(config);
        if (config.geometry.blocks != 0 && config.geometry.pages_per_block != 0) {
            const std::uint32_t last_page =
                config.geometry.blocks * config.geometry.pages_per_block - 1;
            assert(mapper.nand_page_offset(last_page) + page_size == expected);
            assert(mapper.nand_block_size_bytes() ==
                   static_cast<std::uint64_t>(config.geometry.pages_per_block) * page_size);
        }

        const std::uint64_t expected_otp =
            static_cast<std::uint64_t>(config.constraints.otp_page_count) * page_size;
        if (config.capabilities.otp && config.constraints.otp_page_count != 0) {
            assert(model.otp_storage_size_bytes() == expected_otp);
        }
    } else {
        assert(config.total_size_bytes() == config.geometry.memory_size);
        assert(config.effective_page_size() == config.geometry.page_size);
        assert(model.storage_size_bytes() == config.geometry.memory_size);
        assert(model.otp_storage_size_bytes() == 0);
    }
}

} // namespace

int main()
{
    const std::vector<std::string> profiles = {
        "configs/demo_nor.yaml",
        "configs/demo_nor_bp.yaml",
        "configs/demo_nand.yaml",
        "configs/nor_by25q64as.yaml",
        "configs/nor_gd25le128e.yaml",
        "configs/nor_m25p40.yaml",
        "configs/nor_mx25l25645g.yaml",
        "configs/nor_w25q32jv.yaml",
        "configs/nand_gd5f1gm7ue.yaml",
        "configs/nand_mt29f2g01.yaml",
        "configs/nand_w25n01gv.yaml",
    };

    for (const std::string& profile : profiles) {
        check_profile(profile);
    }

    std::cout << "Storage layout tests PASS\n";
    return 0;
}
