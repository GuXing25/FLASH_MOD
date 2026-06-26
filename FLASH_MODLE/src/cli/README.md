# src/cli

本目录放命令行入口和 demo 自测流程。

## 文件说明

- `main.cpp`：加载配置，打印模型 profile，运行 NOR/SPI-NAND self-test。

## 允许放入

- CLI 参数处理。
- demo 输出。
- smoke test 所需的命令序列。

## 不应放入

- 模型核心行为。
- capability 规则。
- 配置验证规则。

CLI 是验证和演示入口，不是模型逻辑本体。
