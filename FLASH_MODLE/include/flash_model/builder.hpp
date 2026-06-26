#ifndef FLASH_MODEL_BUILDER_HPP
#define FLASH_MODEL_BUILDER_HPP

#include "flash_model/model.hpp"
#include "flash_model/validator.hpp"

#include <string>

namespace flash_model {

ValidationReport validate_model_config(ModelConfig config); // 应用 policy 默认值后验证配置。
FlashModel build_model(ModelConfig config); // 从内存配置组装 FlashModel。
FlashModel build_model_from_file(const std::string& path); // 从配置文件加载并组装 FlashModel。

} // namespace flash_model

#endif
