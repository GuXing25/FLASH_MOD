# FLASH_MOD_DIV

`FLASH_MOD_DIV` 是从整合版 `FLASH_MOD_TOG` 拆分出来的分类项目。

目录结构：

```text
FLASH_MOD_DIV/
  NOR/    SPI-NOR / NOR-only 模型项目
  NAND/   SPI-NAND / NAND-only 模型项目
```

拆分原则：

- 两个子项目保留相同的模块骨架：`config_parser -> flash_hardware -> flash_event -> flash_core -> ftl -> flash_test -> main`。
- `NOR` 只保留 NOR 配置，并在配置解析阶段拒绝 NAND profile。
- `NAND` 只保留 SPI-NAND/RAW-NAND 配置，并在配置解析阶段拒绝 NOR profile。
- 公共接口暂时保持一致，方便以后对照整合版继续扩展或回归测试。

整合版项目已复制到：

```text
../FLASH_MOD_TOG
```
