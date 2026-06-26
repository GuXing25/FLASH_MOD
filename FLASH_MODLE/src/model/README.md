# src/model

本目录放命令级模型外观层。`FlashModel` 是统一框架对外执行命令的主入口。

## 文件说明

- `model.cpp`：实现 `FlashModel` 的命令接口和命令流程编排。

## 角色

`FlashModel` 负责把配置、Common Core、Capability Modules 和 Device Policy 串起来，形成可执行模型。

它可以：

- 做命令级流程编排。
- 调用 `src/core` 完成地址、寄存器、时序、存储和接口耗时。
- 调用 capability hook 判断命令是否允许。
- 使用 policy 设置上电默认状态和少量器件族规则。

它不应该：

- 长期承载某个厂商的大量私有行为。
- 直接塞入复杂 capability 细节。
- 复制 core 已有的地址或存储规则。

## 当前命令覆盖

- 共有命令：`read_id`、`write_enable`、`write_disable`、`wait_ready`、`reset`、`suspend`、`resume`、`read_unique_id`。
- NOR 命令：`nor_read`、`nor_program`、`nor_sector_erase`、`nor_block32_erase`、`nor_block_erase`、`nor_chip_erase`。
- NOR 扩展：`deep_power_down`、`release_power_down`、`enter_4byte_address`、`exit_4byte_address`、`read_sfdp`。
- NOR security register：`read_security_register`、`program_security_register`、`erase_security_register`、`lock_security_register`。
- SPI-NAND 命令：`page_read`、`read_from_cache`、`program_load`、`program_execute`、`block_erase`、`copy_back`。
- SPI-NAND 扩展：`enter_otp_mode`、`exit_otp_mode`、`get_feature`、`set_feature`、`select_die`、`select_plane`、`set_read_retry_level`、`read_parameter_page`。

## 维护原则

`FlashModel` 应保持“薄而清楚”。命令流程可以在这里，但规则细节应尽量下沉：

- 通用机制下沉到 `src/core`。
- 可选功能下沉到 `src/capabilities`。
- 厂商族特例下沉到 `src/policies`。
- 纯参数差异留在 `configs/`。
