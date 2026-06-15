# FLASH_MOD_DIV / NOR

这是从整合版 `FLASH_MOD_TOG` 拆出的 NOR-only 模型项目，面向 SPI-NOR / NOR 类 Flash。

## 模块结构

```text
config_parser -> flash_hardware -> flash_event -> flash_core -> ftl -> flash_test -> main
```

该项目只接受 `FLASH_CLASS NOR` 或 `FLASH_CLASS SPI_NOR`。如果传入 SPI-NAND / RAW-NAND 配置，`config_parser` 会直接报错。

## 保留配置

```text
configs/by25q64as.conf
configs/demo_nor.conf
configs/gd25l_simplified.conf
configs/m25p40.conf
configs/mx25l25645g.conf
configs/w25q32jv.conf
```

## 构建运行

```bash
cd ~/project/FLASH_MOD/FLASH_MOD_DIV/NOR
make
make run
make run-w25q
```

也可以指定配置：

```bash
./flash_mod_nor_sim configs/mx25l25645g.conf --self-test
```

## 当前 NOR 行为

- direct array read。
- page program。
- sector / 32K / 64K / chip erase。
- WEL/WIP 状态。
- 上电 tPUW 写保护窗口。
- 页内 wrap 可配置。
- program 使用 `old & new`，只允许 `1 -> 0`。
