#include "flash_model/address.hpp"
#include "flash_model/loader.hpp"

#include <cassert>
#include <iostream>

using namespace flash_model;

int main()
{
    ModelConfig nor = load_config_file("configs/demo_nor_bp.yaml");
    AddressMapper nor_mapper(nor);
    assert(nor_mapper.nor_sector_base(0x0000) == 0x0000);
    assert(nor_mapper.nor_sector_base(0x0123) == 0x0000);
    assert(nor_mapper.nor_sector_base(0x1000) == 0x1000);
    assert(nor_mapper.nor_block32_base(0x0000) == 0x0000);
    assert(nor_mapper.nor_block32_base(0x1234) == 0x0000);
    assert(nor_mapper.nor_block32_base(0x8000) == 0x8000);
    assert(nor_mapper.nor_block_base(0x0000) == 0x0000);
    assert(nor_mapper.nor_block_base(0x1234) == 0x0000);
    assert(nor_mapper.nor_range_protected(0, 1));
    assert(nor_mapper.nor_range_protected(4095, 1));
    assert(!nor_mapper.nor_range_protected(4096, 1));
    assert(AddressMapper::ranges_overlap(0, 10, 9, 1));
    assert(!AddressMapper::ranges_overlap(0, 10, 10, 1));

    ModelConfig nand = load_config_file("configs/demo_nand.yaml");
    AddressMapper nand_mapper(nand);
    assert(nand_mapper.nand_total_pages() == 16);
    assert(nand_mapper.nand_block_of_page(7) == 1);
    assert(nand_mapper.nand_first_page_of_block(2) == 8);
    assert(nand_mapper.nand_page_offset(2) == 544);
    assert(nand_mapper.nand_block_offset(1) == 1088);
    assert(nand_mapper.nand_block_size_bytes() == 1088);

    std::cout << "AddressMapper tests PASS\n";
    return 0;
}
