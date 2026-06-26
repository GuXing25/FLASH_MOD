# FLASH_MODLE

`FLASH_MODLE` 是统一 Flash 模型框架的实验目录。目录名沿用早期命名，代码、可执行文件和命名空间统一使用 `flash_model`。

本项目的最终目标是：外部 AI 从不同 Flash 数据手册中提取必要参数、命令、寄存器、时序、约束和证据后，生成结构化配置文件；本框架读取配置，通过统一内核、能力模块和器件策略组合出对应的 Flash 行为模型。

```text
External AI
  |
  v
Structured Config + Source Evidence
  |
  v
Config Validator
  |
  v
Flash Model Builder
  |
  +-- Common Core
  +-- Capability Modules
  +-- Device Policy
  |
  v
FlashModel
```

## 目录结构

```text
include/flash_model/   公共头文件；定义配置、模型、core、capability、policy 接口
src/config/            配置对象、配置加载器、配置验证器
src/core/              公共内核：地址映射、命令事务、接口计时、寄存器、存储、时序
src/capabilities/      可选能力模块：block protect、ECC、bad block、copy-back 等
src/policies/          器件策略层：generic_nor、generic_spinand、厂商族策略占位
src/model/             FlashModel 命令级外观层，串接 core/capability/policy
src/builder/           从配置构建模型的入口
src/cli/               命令行 demo 和 smoke 自测入口
configs/               demo profile、datasheet profile、负例 profile
schema/                配置 schema 草案
docs/                  路线、能力边界、分层说明、项目现状
tests/                 单元测试和端到端 smoke test
```

每个代码目录都有自己的 `README.md`。新增代码前应先看对应目录说明，确认功能应该落在配置层、公共内核、能力模块、策略层还是模型外观层。

## 构建和测试

```bash
cd /home/wsl/project/FLASH_MOD/FLASH_MODLE
make
make run-nor
make run-nor-protect
make run-nand
make test
make clean
```

正式可执行文件名是：

```bash
./flash_model_sim
```

早期的 `flash_modle_sim` 只作为历史构建产物在 `make clean` 中清理，不再作为正式入口。

## 当前能力

- 配置 schema 已覆盖 `device`、`geometry`、`timing`、`interface`、`commands`、`registers`、`capabilities`、`constraints`、`transactions`、`policy`、`source`、`field_evidence`。
- Validator 能检查 NOR/SPI-NAND 几何字段、命令和能力是否冲突、证据是否缺失、transaction alias 是否合法。
- Common Core 已具备地址映射、稀疏存储、1->0 program 语义、busy/wait-ready、suspend/resume、寄存器状态和接口传输耗时。
- Capability Modules 已覆盖 block protect、ECC reserved、bad block、copy-back、OTP、security register、unique ID、SFDP、parameter page、die/plane select、read retry 等能力的校验或 hook。
- Device Policy 已有 generic NOR/SPI-NAND，并为 Winbond、Micron、GigaDevice、BOYA、Macronix 等厂商族预留策略落点。
- FlashModel 已实现 NOR 读写擦、security register、deep power-down、4-byte address、SFDP、SPI-NAND page/cache/program/copy-back/OTP/parameter page 等行为。

## 设计原则

- 纯参数差异进入 `configs/` 和 `ModelConfig`。
- 多芯片共享但不是所有芯片都有的功能进入 `src/capabilities/`。
- 少量厂商或器件族私有规则进入 `src/policies/`。
- NAND/NOR 都共享的基础机制进入 `src/core/`。
- 命令流程编排放在 `src/model/`，不要把厂商表格和大量私有规则堆进 `FlashModel`。

推荐比例是：

```text
80% 通用行为：配置化
15% 可选能力：模块化
5% 厂商特例：policy 化
```

## 重要文档

- [总路线.md](总路线.md)：项目路线和阶段目标。
- [docs/代码分层说明.md](docs/代码分层说明.md)：每层职责和新增代码落点。
- [docs/当前能力边界.md](docs/当前能力边界.md)：当前已支持和暂不支持内容。
- [docs/能力矩阵模板.md](docs/能力矩阵模板.md)：后续整理芯片能力矩阵的模板。
- [docs/command_transaction_layer.md](docs/command_transaction_layer.md)：命令事务层说明。
- [docs/chiplet_interface_model.md](docs/chiplet_interface_model.md)：接口计时模型说明。
- [docs/项目现状.md](docs/项目现状.md)：项目进程和阶段记录。
