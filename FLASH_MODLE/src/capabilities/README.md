# src/capabilities

本目录放可选能力模块。能力模块用于表达“多颗芯片会有，但不是所有芯片都有”的行为。

## 文件说明

- `capability.cpp`：能力模块工厂、默认 hook、已实现能力模块。

当前已落地：

- `block_protect`
- `deep_power_down`
- `unique_id`
- `sfdp`
- `parameter_page`
- `security_register`
- `ecc_status`
- `bad_block_management`
- `copy_back`

## 2026-06 Incremental Coverage

- `otp`: validated for NAND configs and connected to `FlashModel` OTP mode.
- `suspend_resume`: validated against `commands.suspend/resume` and backed by `TimingEngine`.
- `security_register`: validates configurable count/size and supports model-level lock.
- `die_select` / `plane_select`: validate geometry and expose runtime selection state.
- `read_retry`: validates `constraints.read_retry_levels` and exposes runtime retry level.
- `unique_id`: validates the read-unique-id command and configured ID length for NOR and SPI-NAND profiles.
- `sfdp`: validates the NOR SFDP command and configured SFDP byte range.
- `parameter_page`: validates the SPI-NAND parameter-page command and configured parameter-page size.

Capability modules still avoid vendor tables. Detailed vendor rules, such as BP
bit decoding or OTP lock-bit encoding, belong in `src/policies`.

## 允许放入

- 新 capability 的 validate/attach/hook 实现。
- 与能力强相关的状态检查。
- 能力自己的小型规则，例如 ECC reserved 区检查、坏块拒写拒擦。

## 不应放入

- 通用地址公式，放到 `src/core/address.cpp`。
- 通用存储读写，放到 `src/core/storage.cpp`。
- 厂商族特例，放到 `src/policies`。

新增能力时，应同步补：

- `CapabilityConfig` 字段。
- schema 字段。
- demo config。
- capability 单元测试或 smoke test。
