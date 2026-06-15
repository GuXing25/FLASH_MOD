# FLASH_MOD_DIV / NAND

这是从整合版 `FLASH_MOD_TOG` 拆出的 NAND-only 模型项目，当前重点面向 SPI-NAND。

## 模块结构

```text
config_parser -> flash_hardware -> flash_event -> flash_core -> ftl -> flash_test -> main
```

该项目只接受 `FLASH_CLASS SPI_NAND`、`FLASH_CLASS SPINAND`、`FLASH_CLASS RAW_NAND` 或 `FLASH_CLASS NAND`。如果传入 NOR / SPI-NOR 配置，`config_parser` 会直接报错。

## 保留配置

```text
configs/demo_spinand.conf
configs/gd5f1gm7.conf
configs/mt29f2g01.conf
configs/w25n01gv.conf
```

## 构建运行

```bash
cd ~/project/FLASH_MOD/FLASH_MOD_DIV/NAND
make
make run
make run-w25n
```

也可以指定配置：

```bash
./flash_mod_nand_sim configs/gd5f1gm7.conf --self-test
```

## 当前 NAND 行为

- page read -> cache -> read from cache。
- program load / random program load -> program execute。
- block erase。
- Feature A0/B0/C0/D0 默认值。
- WEL/BUSY、P_FAIL/E_FAIL 状态。
- ECC reserved spare 区限制。
- partial program 次数和顺序编程约束。
- program 使用 `old & new`，只允许 `1 -> 0`。
