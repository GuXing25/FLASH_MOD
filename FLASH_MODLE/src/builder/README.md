# src/builder

本目录放模型组装入口。Builder 是配置世界和运行时模型世界的边界。

## 文件说明

- `builder.cpp`：实现 `validate_model_config()`、`build_model()`、`build_model_from_file()`。

## 构建流程

```text
load config
  -> create policy
  -> apply policy defaults
  -> validate config
  -> create capability modules
  -> validate capability modules
  -> validate policy
  -> build FlashModel
```

## 入口说明

- `build_model_from_file(path)`：从配置文件加载 profile 并构建模型。
- `build_model(config)`：从已经提取好的内存配置构建模型。
- `validate_model_config(config)`：只验证配置，不构建运行时模型。

## 约束

- Builder 只负责组装，不执行命令。
- 非法配置应尽量在 builder 阶段被拒绝。
- 新增 policy 或 capability 时，应通过工厂函数接入，不在 builder 中写大量分支。
- 外部 AI 最终应输出结构化配置对象或 profile 文件，builder 负责把它组合成 `FlashModel`。
