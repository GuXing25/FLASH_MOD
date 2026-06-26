# src/policies

本目录放 Device Policy，用于处理少量无法纯配置化、也不适合做成通用能力模块的器件族规则。

## 文件说明

- `policy.cpp`：policy 工厂、generic NOR/SPI-NAND policy、厂商族 policy 占位。

## 允许放入

- `generic_nor` 和 `generic_spinand` 默认规则。
- Winbond/Micron/GigaDevice/BOYA/Macronix 等 family-level 行为差异。
- 少量 chip override。

## 不应放入

- 完整单芯片模型。
- 大量命令流程复制。
- 能够配置化或 capability 化的行为。

判断标准：

- 纯参数差异进 config。
- 多芯片共享可选功能进 capabilities。
- 最后一小部分厂商私有规则才进 policies。

## 2026-06 Policy Placeholders

- `boya_family`: currently uses generic NOR behavior and reserves a home for
  BY25Q64AS BP4..BP0/CMP/TB protection-table rules.
- `macronix_family`: currently uses generic NOR behavior and reserves a home for
  MX25L25645G WPSEL/SPB/DPB and secured-OTP rules.
