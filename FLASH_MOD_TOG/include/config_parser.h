#ifndef FLASH_MOD_CONFIG_PARSER_H
#define FLASH_MOD_CONFIG_PARSER_H

#include "flash_config.h"

#include <string>

namespace flashmod {

// 从 KEY VALUE / KEY=VALUE 风格配置文件加载 DeviceProfile。解析器只负责把
// 数据手册参数转成统一画像，不执行任何 Flash 命令。
DeviceProfile load_profile(const std::string& filename);

// 打印关键画像字段，方便确认当前仿真使用的是哪类器件和几何结构。
void print_profile(const DeviceProfile& profile);

} // namespace flashmod

#endif
