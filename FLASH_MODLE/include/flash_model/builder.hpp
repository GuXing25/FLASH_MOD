#ifndef FLASH_MODEL_BUILDER_HPP
#define FLASH_MODEL_BUILDER_HPP

#include "flash_model/model.hpp"
#include "flash_model/validator.hpp"

#include <string>

namespace flash_model {

ValidationReport validate_model_config(ModelConfig config);
FlashModel build_model(ModelConfig config);
FlashModel build_model_from_file(const std::string& path);

} // namespace flash_model

#endif
