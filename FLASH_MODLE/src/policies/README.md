# src/policies

本目录放 Device Policy。Policy 用于承载少量无法纯配置化、也不适合做成通用能力模块的器件族规则。

## 文件说明

- `policy.cpp`：policy 工厂、generic NOR/SPI-NAND policy、厂商族 policy 占位。

## 当前策略

- `generic_nor`：NOR 通用默认行为。
- `generic_spinand`：SPI-NAND 通用默认行为，例如上电 block protect 默认值。
- `winbond_family`：Winbond 系列策略落点。
- `micron_family`：Micron 系列策略落点。
- `gigadevice_family`：GigaDevice 系列策略落点。
- `boya_family`：BOYA 系列策略落点。
- `macronix_family`：Macronix 系列策略落点。

## 可以放入

- 厂商族默认寄存器规则。
- 厂商族保护位解释。
- 少量无法通过 config/capability 表达的私有限制。
- 与某个 family 共享的 reset/on-power 行为。

## 不应放入

- 完整单芯片模型。
- 大量复制的命令流程。
- 可以配置化的纯参数。
- 多芯片共享的可选能力。

## 判断标准

- 纯参数差异进入 config。
- 多芯片共享可选功能进入 capabilities。
- NAND/NOR 共有基础机制进入 core。
- 最后一小部分厂商或 family 私有规则才进入 policies。
