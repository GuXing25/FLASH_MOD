#include "flash_model/config.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace flash_model {
namespace {

std::string upper(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

} // namespace

std::string to_string(FlashClass cls)
{
    switch (cls) {
    case FlashClass::Nor:
        return "NOR";
    case FlashClass::SpiNand:
        return "SPI_NAND";
    case FlashClass::RawNand:
        return "RAW_NAND";
    }
    return "UNKNOWN";
}

FlashClass flash_class_from_string(const std::string& text)
{
    const std::string value = upper(text);
    if (value == "NOR" || value == "SPI_NOR") return FlashClass::Nor;
    if (value == "SPI_NAND" || value == "SPINAND") return FlashClass::SpiNand;
    if (value == "RAW_NAND" || value == "NAND") return FlashClass::RawNand;
    throw std::runtime_error("unknown flash class: " + text);
}

bool is_nand_like(FlashClass cls)
{
    return cls == FlashClass::SpiNand || cls == FlashClass::RawNand;
}

std::uint32_t ModelConfig::effective_page_size() const
{
    if (geometry.page_size != 0) return geometry.page_size;
    if (is_nand_like(device.cls)) return geometry.main_size + geometry.spare_size;
    return 0;
}

std::uint64_t ModelConfig::total_size_bytes() const
{
    if (!is_nand_like(device.cls)) return geometry.memory_size;
    return static_cast<std::uint64_t>(geometry.blocks) *
           static_cast<std::uint64_t>(geometry.pages_per_block) *
           static_cast<std::uint64_t>(effective_page_size());
}

} // namespace flash_model
