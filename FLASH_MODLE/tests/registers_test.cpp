#include "flash_model/loader.hpp"
#include "flash_model/registers.hpp"

#include <cassert>
#include <iostream>

using namespace flash_model;

int main()
{
    ModelConfig nand = load_config_file("configs/demo_spinand.yaml");
    RegisterEngine regs(nand);

    assert(regs.feature(0xA0) == 0x7C);
    assert(regs.get_feature(0xC0, false) == 0x00);

    regs.set_write_enable(true);
    assert(regs.wel());
    assert((regs.state().status1 & 0x02u) != 0);
    assert((regs.get_feature(0xC0, false) & 0x02u) != 0);

    regs.set_program_fail(true);
    assert((regs.get_feature(0xC0, false) & 0x08u) != 0);

    regs.set_erase_fail(true);
    assert((regs.get_feature(0xC0, true) & 0x05u) == 0x05u);

    regs.reset();
    assert(!regs.wel());
    assert(regs.feature(0xA0) == 0x7C);
    assert(regs.get_feature(0xC0, false) == 0x00);

    ModelConfig nor = load_config_file("configs/demo_nor.yaml");
    RegisterEngine nor_regs(nor);
    assert(nor_regs.state().status2 == 0x02);
    assert(nor_regs.state().quad_enabled);

    std::cout << "RegisterEngine tests PASS\n";
    return 0;
}
