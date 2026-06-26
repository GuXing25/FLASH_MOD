#include "flash_model/capability.hpp"
#include "flash_model/loader.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>

using namespace flash_model;

namespace {

const CapabilityModule* find_module(const std::vector<std::unique_ptr<CapabilityModule>>& modules,
                                    const std::string& name)
{
    for (const auto& module : modules) {
        if (module->name() == name) return module.get();
    }
    return nullptr;
}

} // namespace

int main()
{
    ModelConfig nor = load_config_file("configs/demo_nor_bp.yaml");
    std::vector<std::unique_ptr<CapabilityModule>> nor_modules = create_capability_modules(nor);
    const CapabilityModule* nor_bp = find_module(nor_modules, "block_protect");
    assert(nor_bp != nullptr);

    RuntimeState nor_state;
    nor_bp->attach(nor_state);
    assert(!nor_state.enabled_capabilities.empty());
    assert(nor_bp->before_nor_program(nor, nor_state, 0, 16).ok == false);
    assert(nor_bp->before_nor_erase(nor, nor_state, 0, 4096).ok == false);
    assert(nor_bp->before_nor_program(nor, nor_state, 4096, 16).ok);

    ModelConfig nor_dpd = load_config_file("configs/demo_nor.yaml");
    std::vector<std::unique_ptr<CapabilityModule>> nor_dpd_modules =
        create_capability_modules(nor_dpd);
    const CapabilityModule* dpd = find_module(nor_dpd_modules, "deep_power_down");
    assert(dpd != nullptr);
    const CapabilityModule* security = find_module(nor_dpd_modules, "security_register");
    assert(security != nullptr);

    ModelConfig nand = load_config_file("configs/demo_nand.yaml");
    std::vector<std::unique_ptr<CapabilityModule>> nand_modules = create_capability_modules(nand);
    const CapabilityModule* nand_bp = find_module(nand_modules, "block_protect");
    assert(nand_bp != nullptr);

    RuntimeState locked;
    locked.features[0xA0] = 0x7C;
    assert(!nand_bp->before_nand_block_erase(nand, locked, 0).ok);
    assert(!nand_bp->before_nand_program_execute(nand, locked, 0, 0, false).ok);

    RuntimeState unlocked;
    unlocked.features[0xA0] = 0x00;
    assert(nand_bp->before_nand_block_erase(nand, unlocked, 0).ok);
    assert(nand_bp->before_nand_program_execute(nand, unlocked, 0, 0, false).ok);

    const CapabilityModule* ecc = find_module(nand_modules, "ecc_status");
    assert(ecc != nullptr);
    assert(ecc->marks_nand_program_load_violation(nand, unlocked, 256, 4));
    assert(!ecc->marks_nand_program_load_violation(nand, unlocked, 0, 4));
    assert(!ecc->before_nand_program_execute(nand, unlocked, 0, 0, true).ok);
    assert(ecc->before_nand_program_execute(nand, unlocked, 0, 0, false).ok);

    const CapabilityModule* bad_block = find_module(nand_modules, "bad_block_management");
    assert(bad_block != nullptr);
    RuntimeState bad_block_state;
    bad_block->attach(bad_block_state);
    assert(std::find(bad_block_state.bad_blocks.begin(),
                     bad_block_state.bad_blocks.end(),
                     3) != bad_block_state.bad_blocks.end());
    assert(!bad_block->before_nand_block_erase(nand, bad_block_state, 3).ok);
    assert(bad_block->before_nand_block_erase(nand, bad_block_state, 0).ok);
    assert(!bad_block->before_nand_program_execute(nand, bad_block_state, 12, 3, false).ok);
    assert(bad_block->before_nand_program_execute(nand, bad_block_state, 0, 0, false).ok);

    const CapabilityModule* copy_back = find_module(nand_modules, "copy_back");
    assert(copy_back != nullptr);
    RuntimeState copy_state;
    copy_back->attach(copy_state);
    assert(!copy_state.enabled_capabilities.empty());
    assert(!copy_back->before_nand_copy_back(nand, copy_state, 0, 0, 0).ok);
    assert(copy_back->before_nand_copy_back(nand, copy_state, 0, 1, 0).ok);

    std::cout << "CapabilityModule tests PASS\n";
    return 0;
}
