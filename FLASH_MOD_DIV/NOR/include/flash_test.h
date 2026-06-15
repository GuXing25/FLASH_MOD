#ifndef FLASH_MOD_FLASH_TEST_H
#define FLASH_MOD_FLASH_TEST_H

#include "ftl.h"

namespace flashmod {

// 根据 profile().is_nand_like() 自动选择 NOR 或 SPI-NAND 自检流程。
// 自检覆盖 read-id、erase、program、read、old&new，以及 SPI-NAND cache 路径。
bool flash_test_run(FTL& ftl);

} // namespace flashmod

#endif
