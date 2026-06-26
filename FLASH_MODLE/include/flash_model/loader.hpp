#ifndef FLASH_MODEL_LOADER_HPP
#define FLASH_MODEL_LOADER_HPP

#include "flash_model/config.hpp"

#include <string>

namespace flash_model {

ModelConfig load_config_file(const std::string& path);

} // namespace flash_model

#endif
