#include "flash_model/loader.hpp"
#include "flash_model/validator.hpp"

#include <cassert>
#include <iostream>

using namespace flash_model;

namespace {

bool has_level(const ValidationReport& report, ValidationLevel level)
{
    for (const auto& message : report.messages) {
        if (message.level == level) return true;
    }
    return false;
}

} // namespace

int main()
{
    ModelConfig nor = load_config_file("configs/demo_nor.yaml");
    assert(nor.has_evidence("device.name"));
    assert(nor.has_evidence("geometry.memory_size"));
    ValidationReport nor_report = validate_config(nor);
    assert(!has_level(nor_report, ValidationLevel::EvidenceGap));

    ModelConfig nand = load_config_file("configs/demo_nand.yaml");
    assert(nand.has_evidence("constraints.ecc_reserved_start"));
    assert(nand.has_evidence("constraints.ecc_reserved_end"));
    ValidationReport nand_report = validate_config(nand);
    assert(!has_level(nand_report, ValidationLevel::EvidenceGap));

    ModelConfig missing = load_config_file("configs/nand_no_evidence.yaml");
    ValidationReport missing_report = validate_config(missing);
    assert(missing_report.ok());
    assert(has_level(missing_report, ValidationLevel::EvidenceGap));

    std::cout << "Evidence validator tests PASS\n";
    return 0;
}
