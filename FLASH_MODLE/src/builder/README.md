# src/builder

本目录放模型组装入口。

## 文件说明

- `builder.cpp`：实现 `build_model_from_file()`。

## 构建流程

```text
load config
  -> create policy
  -> apply policy defaults
  -> validate config
  -> create capability modules
  -> validate capability/policy
  -> build FlashModel
```

## 约束

- builder 只负责组装，不执行命令。
- builder 是配置到模型的边界，任何非法配置都应尽量在这里之前被 validator 拦住。
- 新增 policy 或 capability 时，优先通过工厂函数接入，不在 builder 中写大量分支。
## 2026-06 Builder API

The builder layer has two entry paths:

- `build_model_from_file(path)`: load a YAML-like profile and build a model.
- `build_model(config)`: build directly from an already extracted `ModelConfig`.
- `validate_model_config(config)`: apply policy/module validation without
  constructing the runtime model.

This supports the final flow where an external AI extracts a structured config
object first, then hands that object to the Flash framework.
