# FLASH_MOD

`FLASH_MOD` 是把本项目里多个 Flash 模型抽象后的统一命令级仿真框架。

现在的代码结构刻意贴近现有 `w25n` 这类模型的分层，只是把它做成可配置、可复用版本：

```text
config_parser -> flash_hardware -> flash_event -> flash_core -> ftl -> flash_test -> main
```

其中 `FTL` 就是上层调度入口；不再单独拆出额外调度模块。

## 目录

```text
include/                 统一头文件
src/config_parser.cpp    KEY VALUE 配置解析
src/addr_mapping.cpp     地址归一化、页/块/plane 解析、offset 计算
src/storage_backend.cpp  二进制阵列镜像文件读写擦
src/flash_hardware.cpp   存储区、页/块元数据、storage 后端组合
src/flash_event.cpp      上层请求事件定义
src/flash_core.cpp       NOR / SPI-NAND 器件行为核心
src/ftl.cpp              FTL 队列和调度
src/flash_test.cpp       内置自检
src/main.cpp             程序入口
configs/                 demo 和现有芯片画像
docs/CONFIG_SCHEMA.md    配置字段说明
docs/ARCHITECTURE.md     模块职责和调用关系
```

## 当前统一能力

- NOR direct array read / page program / sector/block/chip erase。
- SPI-NAND page read -> cache -> read from cache。
- SPI-NAND program load -> program execute。
- 擦除态 `0xFF`，program 使用 `old & new`。
- WEL/WIP 或 OIP/BUSY 状态。
- pending busy：`AUTO_COMPLETE=0` 时需要 `wait_ready()` 或等待时间推进。
- 上电写禁止窗口 `T_PUW_US`。
- NAND block protect 默认值、Feature A0/B0/C0/D0。
- NAND ECC reserved spare 区限制。
- NAND partial program 次数和顺序编程约束。
- 二进制 storage 文件后端。

## 构建运行

```bash
cd ~/project/FLASH_MOD
make
make run        # demo NOR
make run-nand   # demo SPI-NAND
```

也可以指定配置：

```bash
./flash_mod_sim configs/w25q32jv.conf --self-test
./flash_mod_sim configs/gd5f1gm7.conf --self-test
```

## 配置与实现的分工

配置文件描述器件画像：

- `FLASH_CLASS`: `NOR` / `SPI_NAND` / `RAW_NAND`
- 几何结构：容量、页、main/spare、块、plane
- 时序：读、编程、擦除、复位、总线频率
- 默认状态：QE、ECC、保护寄存器、Feature Register
- 行为开关：是否强制顺序编程、是否要求 WEL、是否默认全保护

代码固定共同 flash 语义：

- 擦除恢复为 `0xFF`
- 编程只能 `1 -> 0`
- 写/擦需要 WEL
- 忙周期和状态位
- NOR 与 SPI-NAND 两类数据路径

厂商私有命令如果无法用现有字段表达，建议新增配置字段或策略分支，不要复制整套模型。

## 现有配置画像

```text
demo_nor.conf          小容量 NOR 自检
demo_spinand.conf      小容量 SPI-NAND 自检
m25p40.conf            M25P40 NOR
w25q32jv.conf          W25Q32JVSSIQ NOR
by25q64as.conf         BY25Q64AS NOR
mx25l25645g.conf       MX25L25645G NOR
gd25l_simplified.conf  GD25L 简化 NOR
w25n01gv.conf          W25N01GV SPI-NAND
mt29f2g01.conf         MT29F2G01 SPI-NAND
gd5f1gm7.conf          GD5F1GM7 SPI-NAND
```

## 边界

第一版是统一框架，不是每个厂商模型的完整替代品。它保留了共同数据路径和关键约束；像 MX25L 的完整 SPB/DPB/QPI、W25Q 的 WPS 独立锁、GD5F 的完整 copy-back、Micron cache random/last 等，可以作为后续策略模块继续接入。
