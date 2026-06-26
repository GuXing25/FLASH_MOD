# src/core

本目录是 Common Core，保存 NAND/NOR 共同使用的底层行为机制。这里的代码应尽量稳定、通用、无厂商倾向。

## 文件说明

- `address.cpp`：地址映射，负责 NOR sector/block 对齐和 NAND page/block offset 计算。
- `command.cpp`：命令事务层，描述 opcode、address、dummy、payload、response、alias 等传输形状。
- `interface.cpp`：行为级接口计时，把命令事务转换为耗时。
- `registers.cpp`：WEL、status register、feature register、P_FAIL/E_FAIL 等基础状态。
- `storage.cpp`：稀疏存储后端，默认擦除态 `0xFF`，program 使用 `old & new` 的 1->0 语义。
- `timing.cpp`：模型时间、busy 窗口、wait-ready、suspend/resume。

## 可以放入

- 多类 Flash 共享的基础机制。
- 与具体厂商无关的地址、时序、存储、寄存器和事务计算。
- 可被 NOR 和 NAND 共同复用的工具逻辑。

## 不应放入

- OTP、copy-back、read-retry 等能力细节。
- Winbond、Micron、GigaDevice 等厂商族私有规则。
- CLI demo 或 smoke test 流程。
- 引脚、电气、PHY 训练或 cycle-accurate 总线模型。

## 维护原则

`src/core` 是模型的地基。新增逻辑时要问：它是否不依赖具体芯片、是否能被多个模型复用、是否不会把 policy/capability 的判断提前混入公共层。如果答案不是肯定，应放到其他目录。
