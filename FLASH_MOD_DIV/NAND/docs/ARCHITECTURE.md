# FLASH_MOD_DIV / NAND 架构

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

## NAND-only 边界

- `config_parser` 只接受 `SPI_NAND` / `SPINAND` / `RAW_NAND` / `NAND`。
- `configs/` 只保留 NAND 配置。
- `flash_test` 只执行 page read、cache read、program load、program execute、block erase 流程。
- `flash_core` 当前仍保留公共接口形状，但运行 profile 被限制为 NAND。

## 关键行为

- row/page address 访问阵列，column address 访问 cache。
- `PAGE_READ` 把阵列页搬到 cache，`READ_FROM_CACHE` 再从 cache 取数。
- `PROGRAM_LOAD` 填 cache，`PROGRAM_EXECUTE` 写入目标页。
- erase 以 block 为单位。
- program 在内部完成阶段执行 `old & new`。
- WEL/BUSY/P_FAIL/E_FAIL 同步到 feature register `0xC0`。
- 支持 ECC reserved spare 区、partial program 次数和 strict sequential program。
