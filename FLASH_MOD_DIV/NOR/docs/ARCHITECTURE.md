# FLASH_MOD_DIV / NOR 架构

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
DeviceProfile <--- config_parser <--- configs/*.conf
```

## NOR-only 边界

- `config_parser` 只接受 `NOR` / `SPI_NOR`。
- `configs/` 只保留 NOR 配置。
- `flash_test` 只执行 direct array read、page program、erase、readback 流程。
- `flash_core` 当前仍保留公共接口形状，但运行 profile 被限制为 NOR。

## 关键行为

- byte address 直接访问阵列。
- erase 使目标范围回到 `0xFF`。
- program 在内部完成阶段执行 `old & new`。
- WEL/WIP 同步到 SR1。
- 支持页内 wrap、tPUW、QE 检查和按配置计算总线时间。
