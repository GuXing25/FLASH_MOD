#ifndef FLASH_MODEL_MODEL_BUILDER_HPP
#define FLASH_MODEL_MODEL_BUILDER_HPP

#include "flash_model/flash_model.hpp"

#include <string>

namespace flash_model {

FlashModel build_model_from_file(const std::string& path);

} // namespace flash_model

#endif
