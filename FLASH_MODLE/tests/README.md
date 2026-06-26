# tests

本目录放轻量单元测试和端到端 smoke test。

## 文件说明

- `address_test.cpp`：地址映射测试。
- `storage_test.cpp`：稀疏存储、old&new、wrap、越界异常测试。
- `registers_test.cpp`：寄存器状态测试。
- `timing_test.cpp`：时序状态测试。
- `capability_test.cpp`：能力模块 hook 测试。
- `command_test.cpp`：命令事务层测试。
- `interface_test.cpp`：接口计时测试。
- `evidence_test.cpp`：字段 evidence 和 validator 测试。
- `model_feature_test.cpp`：模型级功能组合测试。
- `smoke.sh`：运行 demo 配置和 datasheet profile 的端到端自测。

## 测试要求

新增能力或 policy 时，应至少补一种测试：

- capability hook 测试。
- validator 负例测试。
- model feature 集成测试。
- smoke test 端到端行为。

测试命名保持短名，例如 `address_test`、`timing_test`、`capability_test`。

## 当前覆盖

- Common Core：地址、存储、寄存器、时序、命令事务、接口耗时。
- Config/Validator：字段证据、非法 NOR/NAND 命令组合。
- Capability：block protect、ECC reserved、bad block、copy-back。
- Model：NOR program/erase、security register、deep power-down、suspend/resume、unique ID、SFDP、SPI-NAND OTP、copy-back、parameter page、die/plane select、read retry。
- Smoke：demo NOR、demo NOR block protect、demo SPI-NAND，以及多个 datasheet-derived profile 的 validate-only。
