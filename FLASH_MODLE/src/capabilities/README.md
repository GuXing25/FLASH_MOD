# src/capabilities

本目录放 Capability Modules。能力模块用于表达“多颗芯片可能拥有，但不是所有芯片都有”的功能。

## 文件说明

- `capability.cpp`：能力模块工厂、默认 hook、已实现能力模块。

## 当前能力

- `block_protect`：NOR 保护窗口和 SPI-NAND A0h block protect 检查。
- `deep_power_down`：低功耗能力校验，实际状态在 `FlashModel` 和 `RegisterEngine` 中维护。
- `suspend_resume`：检查 suspend/resume 命令是否与能力开关一致。
- `unique_id`：检查 unique ID 命令和长度。
- `sfdp`：检查 NOR SFDP 命令和空间大小。
- `otp`：检查 NAND OTP mode 所需命令和页数。
- `security_register`：检查 NOR security register 数量和大小。
- `ecc_status`：标记写入 ECC reserved spare range 的违规。
- `bad_block_management`：挂载初始坏块并拒绝坏块擦写。
- `copy_back`：检查 copy-back 基础约束。
- `die_select` / `plane_select`：检查几何字段是否支持选择。
- `read_retry`：检查 read-retry 档位。
- `parameter_page`：检查 SPI-NAND parameter page 命令和大小。

## 可以放入

- 新 capability 的 validate/attach/hook。
- 与该能力强相关的状态检查。
- 能力自己的小型规则，例如 ECC reserved 区、坏块拒写拒擦、copy-back 基本限制。

## 不应放入

- 通用地址公式，放到 `src/core/address.cpp`。
- 通用存储读写，放到 `src/core/storage.cpp`。
- 厂商保护位表、OTP 锁定位编码、特殊 read retry 表，放到 `src/policies`。

## 新增能力步骤

1. 在 `CapabilityConfig` 增加开关。
2. 在 schema、loader、validator 中同步字段。
3. 在 `CapabilityKind` 中增加枚举和值到名称的映射。
4. 若只需要校验，使用 `SimpleCapabilityModule`。
5. 若需要拦截命令，新增专用 module 并覆盖对应 hook。
6. 增加 capability 单元测试或 smoke 端到端测试。
