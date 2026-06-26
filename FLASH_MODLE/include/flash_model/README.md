# include/flash_model

本目录是框架的公共接口层。外部工具、测试、CLI 和未来其他前端都应优先依赖这里的头文件，而不是直接引用 `src/` 内部实现。

## 文件说明

- `config.hpp`：配置总结构，承载外部 AI 从 datasheet 中提取的器件、几何、时序、命令、能力、约束、策略和证据。
- `loader.hpp`：配置文件加载入口，当前读取 YAML-like profile。
- `validator.hpp`：配置验证报告和通用验证入口。
- `builder.hpp`：从文件或内存配置构建 `FlashModel` 的入口。
- `model.hpp`：命令级模型外观接口。
- `command.hpp`：命令事务描述接口，把 opcode/address/dummy/payload/response 转成传输字节数。
- `interface.hpp`：行为级 chiplet/controller 接口计时接口。
- `address.hpp`：NOR/NAND 地址映射接口。
- `storage.hpp`：稀疏存储后端接口。
- `timing.hpp`：busy、wait-ready、suspend/resume 时序接口。
- `registers.hpp`：状态寄存器和 feature register 接口。
- `capability.hpp`：能力模块 hook 和运行态共享结构。
- `policy.hpp`：器件策略层接口。

## 命名规则

文件名保持短名，因为目录和命名空间已经提供上下文。例如使用 `model.hpp`，不再使用 `flash_model.hpp`；使用 `builder.hpp`，不再使用 `model_builder.hpp`。

## 修改原则

- 新增配置字段时，先改 `config.hpp`，再同步 loader、validator、schema、demo config 和测试。
- 新增命令接口时，先确认它是 NOR、NAND 还是共有命令，再放入 `model.hpp` 对应分组。
- 新增能力 hook 时，优先扩展 `capability.hpp`，不要直接把私有规则塞进 `model.hpp`。
- 新增厂商族规则时，优先扩展 `policy.hpp`。
- 头文件只描述稳定契约，复杂实现留在 `src/`。
