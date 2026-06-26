# src/config

本目录负责结构化配置输入，是外部 AI 输出 profile 进入模型框架的第一站。

## 文件说明

- `config.cpp`：实现 `ModelConfig` 的辅助方法，例如 Flash 类型转换、有效页大小、总容量、字段证据查询。
- `loader.cpp`：读取 YAML-like 配置文件，并填充 `ModelConfig`。
- `validator.cpp`：检查配置是否合法，输出 error、warning、evidence gap。

## 输入和输出

输入是 `configs/*.yaml` 中的结构化 profile。输出是 C++ 内存中的 `ModelConfig` 和 `ValidationReport`。

配置层不执行 Flash 命令，也不读写存储阵列。它只回答两个问题：

- profile 是否能被解析成统一结构。
- profile 是否满足当前模型框架的基本约束。

## 可以放入

- 新 schema 字段的解析。
- 默认值、派生值和字段证据检查。
- NOR/SPI-NAND 类型级约束。
- command/capability/transaction/policy 的一致性检查。

## 不应放入

- 具体命令执行流程。
- 存储阵列读写。
- 厂商私有行为表。
- CLI 输出逻辑。

## 新增字段步骤

1. 在 `include/flash_model/config.hpp` 增加字段和中文注释。
2. 在 `loader.cpp` 中解析该字段。
3. 在 `validator.cpp` 中补合法性检查和 evidence 检查。
4. 在 `schema/flash_model_schema.yaml` 中同步 schema。
5. 在至少一个 `configs/*.yaml` 中给出示例。
6. 在 `tests/` 或 `smoke.sh` 中补验证。
