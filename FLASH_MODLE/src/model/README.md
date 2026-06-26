# src/model

本目录放命令级模型外观层。

## 文件说明

- `model.cpp`：实现 `FlashModel` 的命令接口，例如 read-id、program、sector/block/chip erase、security register、page-read、copy-back。

## 角色

`FlashModel` 负责把配置、core、capability 和 policy 串起来，形成可执行模型。

它可以：

- 做命令级流程编排。
- 调用 `core` 完成地址、寄存器、时序、存储操作。
- 调用 capability hook 判断命令是否允许。
- 使用 policy 做上电默认状态和私有规则校验。

它不应该：

- 长期承载某个厂商的私有行为。
- 直接塞入大量 capability 细节。
- 复制 core 里已有的地址或存储规则。

目标是让 `FlashModel` 保持薄而清楚。
## 2026-06 Command Additions

`FlashModel` now exposes these extra command-level operations:

- `suspend()` / `resume()`
- `enter_4byte_address()` / `exit_4byte_address()`
- `read_unique_id()` / `read_sfdp()`
- `read_parameter_page()`
- `nor_block32_erase()`
- `enter_otp_mode()` / `exit_otp_mode()`
- `lock_security_register()`
- `select_die()` / `select_plane()`
- `set_read_retry_level()`

The model keeps the orchestration here, while detailed rule decisions continue
to flow through capability hooks, policy defaults, and the common core.

## 2026-06 Parameter Page Detail

`read_parameter_page()` now returns a generated NAND parameter page with:

- `ONFI` signature and redundant 256-byte copies.
- Manufacturer/model text from `device`.
- Page geometry, pages per block, blocks per unit, and logical-unit count.
- Bad-block maximum derived from `minimum_valid_blocks`.
- ECC correctability and configured tPROG/tBERS/tR timing fields.
