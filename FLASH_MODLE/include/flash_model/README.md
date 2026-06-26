# include/flash_model

本目录放公共头文件，是框架对其他代码暴露的接口层。

## 文件说明

- `config.hpp`: 结构化配置对象，包括 device/geometry/timing/interface/commands/capabilities/constraints/policy/evidence。
- `loader.hpp`: 配置文件加载入口。
- `validator.hpp`: 配置验证报告和验证入口。
- `builder.hpp`: 从配置构建模型的入口。
- `model.hpp`: 命令级 `FlashModel` 外观接口。
- `command.hpp`: 命令事务描述接口，用于把命令 payload/response 转成接口传输字节数。
- `address.hpp`: 地址映射接口。
- `storage.hpp`: 存储后端接口。
- `timing.hpp`: 时序和 busy 状态接口。
- `registers.hpp`: 寄存器和运行时状态接口。
- `interface.hpp`: 暂定芯粒间 transaction 接口计时。
- `capability.hpp`: 能力模块接口和 hook。
- `policy.hpp`: 器件策略接口。

## 命名规则

文件名尽量短，目录已经表达上下文。例如这里已经处在 `flash_model`
命名空间和 include 目录中，所以使用 `model.hpp`，不使用
`flash_model.hpp`。

## 约束

- 头文件只声明稳定接口，避免放大段实现。
- 新增公共能力时优先扩展 `capability.hpp` 的 hook。
- 新增少量厂商私有规则时优先扩展 `policy.hpp`，不要直接写进 `model.hpp`。
- 芯粒间接口当前只抽象事务耗时；真实协议声明明确后，再扩展 `interface.hpp`。
