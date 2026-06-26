# src/cli

本目录放命令行入口和 demo 自测流程。CLI 是验证和演示入口，不是模型逻辑本体。

## 文件说明

- `main.cpp`：加载配置，打印模型 profile，运行 NOR/SPI-NAND self-test。

## 当前命令

```bash
./flash_model_sim configs/demo_nor.yaml --self-test
./flash_model_sim configs/demo_nand.yaml --self-test
./flash_model_sim configs/nor_w25q32jv.yaml --validate-only
```

## 可以放入

- CLI 参数处理。
- demo 输出。
- smoke test 所需的命令序列。
- 便于人工观察的 profile 打印。

## 不应放入

- 模型核心行为。
- capability 规则。
- 配置验证规则。
- 厂商私有状态机。

如果某个逻辑会影响模型语义，应移动到 `src/model`、`src/core`、`src/capabilities` 或 `src/policies`。
