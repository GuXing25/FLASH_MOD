# tests

本目录放轻量单元测试和端到端 smoke test。

## 文件说明

- `address_test.cpp`：地址映射测试。
- `registers_test.cpp`：寄存器状态测试。
- `timing_test.cpp`：时序状态测试。
- `capability_test.cpp`：能力模块 hook 测试。
- `evidence_test.cpp`：字段 evidence 和 validator 测试。
- `smoke.sh`：运行 demo 配置的端到端命令级自测。

## 测试要求

新增能力或 policy 时，应至少补一种测试：

- capability hook 测试。
- validator 负例测试。
- smoke test 端到端行为。

测试命名保持短名：`address_test`、`timing_test`、`capability_test`。
## 2026-06 Added Coverage

- `model_feature_test.cpp`: end-to-end model checks for suspend/resume,
  security-register lock, NAND OTP storage, plane/die selection, and read-retry
  runtime state.
- `model_feature_test.cpp`: now also checks NOR Unique ID, SFDP signature,
  32KiB block erase, and the Macronix 4-byte address profile.
- `model_feature_test.cpp`: now checks GigaDevice SPI-NAND Unique ID and
  ONFI-style parameter page generation.
- `model_feature_test.cpp`: now checks W25N01GV and MT29F2G01 parameter-page
  geometry, redundant copies, bad-block budget, and ECC capability fields.
- `timing_test.cpp`: now checks `TimingEngine::suspend()` and `resume()`.
- `smoke.sh`: now greps for NOR suspend/resume, security lock rejection, and
  NAND OTP mode behavior. It also validates the W25Q32JV, BY25Q64AS,
  MX25L25645G, GD25LE128E, GD5F1GM7UEYIGR, M25P40, W25N01GV, and
  MT29F2G01 datasheet-derived profiles.
