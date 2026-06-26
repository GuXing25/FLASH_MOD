# src/core

本目录是 Common Core，保存 NAND/NOR 共用的底层行为机制。

## 文件说明

- `address.cpp`: 地址映射，包括 NOR sector/32KiB block/64KiB block base，以及 NAND page/block offset。
- `command.cpp`: 命令事务描述表，把命令名、opcode、dummy、phase lane、alias、payload/response 长度转换成接口传输字节数和可追溯元数据。
- `storage.cpp`: 稀疏存储后端，擦除态为 `0xFF`，编程采用 `old & new` 的 1 -> 0 语义。
- `timing.cpp`: 当前仿真时间、busy 窗口、wait-ready、suspend/resume 时间状态。
- `registers.cpp`: WEL、status register、feature register、P_FAIL/E_FAIL 等基础状态。
- `interface.cpp`: 暂定的芯粒间 transaction 接口计时，把命令/地址/payload/响应传输耗时叠加到 `TimingEngine`。

## 允许放入

- 多类 Flash 共享的行为级基础机制。
- 不依赖具体厂商的状态更新和工具逻辑。
- 可被 NOR 和 NAND 共同复用的存储、地址、命令事务、时序、寄存器、接口计时基础。

## 不应放入

- OTP、copy-back、read-retry 等可选能力的细节。
- Winbond/Micron/GigaDevice 等厂商特例。
- CLI 自测逻辑。
- 引脚、电气、PHY 训练或 cycle-accurate 总线状态机。

核心原则：`core` 提供稳定积木，不知道上层 demo，也不关心具体芯片型号。
