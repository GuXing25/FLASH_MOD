# src/config

本目录负责结构化配置输入，是外部 AI 输出配置进入模型框架的第一站。

## 文件说明

- `config.cpp`：`ModelConfig` 的辅助方法，例如 flash class 转换、总容量计算、字段 evidence 判断。
- `loader.cpp`：读取 YAML-like 配置文件，填充 `ModelConfig`。
- `validator.cpp`：检查配置是否合法，并报告 error、warning、evidence gap。

## 允许放入

- 新 schema 字段的解析。
- 字段默认值和派生值。
- NOR/NAND 类型约束。
- 字段来源证据检查。

## 不应放入

- 具体命令执行逻辑。
- 存储阵列读写逻辑。
- 厂商私有行为。

这些内容应分别放到 `src/model`、`src/core`、`src/capabilities` 或 `src/policies`。
## 2026-06 Schema Additions

The config layer now accepts these extra fields:

- `geometry.dies`
- `geometry.block32_size`
- `timing.block32_erase_us`
- `commands.nor_block32_erase`
- `commands.read_unique_id` / `commands.read_sfdp`
- `commands.read_parameter_page`
- `commands.enter_4byte_address` / `commands.exit_4byte_address`
- `commands.suspend` / `commands.resume`
- `commands.lock_security_register`
- `commands.enter_otp_mode` / `commands.exit_otp_mode`
- `constraints.address_bytes`
- `constraints.security_register_count`
- `constraints.security_register_size`
- `constraints.otp_page_count`
- `constraints.read_retry_levels`
- `constraints.unique_id_size`
- `constraints.sfdp_size`
- `constraints.parameter_page_size`
- `constraints.bad_block_marker_offset`
- `constraints.minimum_valid_blocks`
- `constraints.ecc_bits`
- `device.unique_id`

These fields are intentionally small and explicit. They give the external AI a
place to put datasheet facts without forcing it to generate C++ behavior.
