# FLASH_MOD 模块架构

## 调用关系

```text
main.cpp
  |
  v
flash_test
  |
  v
FTL
  |
  v
FlashEvent --------> FlashCore
                         |
                         v
                    FlashHardware
                    |          |
                    v          v
              AddressMapper  StorageBackend
                    ^
                    |
DeviceProfile <--- config_parser <--- *.conf
```

这和现有 `w25n` 的层次是一致的，只是把 `FlashHardware` 内部再拆出了地址映射和 storage 后端。

## 模块职责

### config_parser

文件：

```text
include/flash_config.h
include/config_parser.h
src/config_parser.cpp
configs/*.conf
```

职责：

- 定义 `DeviceProfile`。
- 从配置文件读取几何参数、时序参数、默认寄存器和行为开关。
- 派生 `page_size`、`total_pages`、`blocks` 等可计算字段。

### flash_hardware

文件：

```text
include/flash_hardware.h
src/flash_hardware.cpp
include/addr_mapping.h
src/addr_mapping.cpp
include/storage_backend.h
src/storage_backend.cpp
```

职责：

- 描述存储区和页/块元数据。
- 管理后端二进制 storage 文件。
- 提供 page/block 到文件 offset 的映射。
- 提供 read/write/erase 存储原语。

这一层只关心“存储阵列和地址”，不解释命令语义。

### flash_event

文件：

```text
include/flash_event.h
src/flash_event.cpp
```

职责：

- 定义 `EventType`。
- 定义 `FlashEvent`，即一条上层请求。
- 定义 `IoMode`、`EraseKind`、`OperationResult` 等通用命令数据结构。

### flash_core

文件：

```text
include/flash_core.h
src/flash_core.cpp
```

职责：

- 实现共同 Flash 物理语义：擦除态、`old & new`、busy、WEL。
- 实现 NOR direct read/program/erase。
- 实现 SPI-NAND cache 两阶段读写。
- 根据配置开关处理 QE、ECC reserved 区、默认保护、partial program 和顺序编程。

### ftl

文件：

```text
include/ftl.h
src/ftl.cpp
```

职责：

- 作为上层 FTL 外观层。
- 提供同步执行 API，例如 `read()`、`program()`、`page_read()`。
- 提供 FIFO 队列：`submit()` + `run()`。

这一层适合继续扩展：

- LBA -> PBA 映射。
- 多 die/plane 调度。
- 坏块替换策略。
- 请求合并。
- 异步事件队列。

### flash_test

文件：

```text
include/flash_test.h
src/flash_test.cpp
```

职责：

- 根据 `FLASH_CLASS` 跑 NOR 或 SPI-NAND 自检。
- 验证 erase/program/read、`old & new`、cache 路径和状态位。

### main

文件：

```text
src/main.cpp
```

职责：

- 读取命令行配置路径。
- 构造 `FlashCore` 和 `FTL`。
- 调用 `flash_test_run()`。

## 推荐扩展方向

- 新增纯配置可表达的芯片：只加 `configs/<chip>.conf`。
- 新增通用行为差异：先扩 `DeviceProfile` 和 config parser，再在 `FlashCore` 接入。
- 新增 FTL 行为：优先扩 `FTL`，不要放进 `FlashCore`。
- 新增地址策略：扩 `AddressMapper`。
- 新增 storage 类型：扩 `StorageBackend` 或抽象出接口。
