# FLASH_MODLE

`FLASH_MODLE` 是下一代统一 Flash 模型框架实验目录。

它验证的核心方向是：

```text
Common Core + Capability Modules + Device Policy
```

详细路线见 [总路线.md](总路线.md)，代码分层见 [docs/代码分层说明.md](docs/代码分层说明.md)。
`datasheet_tq` 提取信息到配置 profile 的映射见 [docs/datasheet_tq_profiles.md](docs/datasheet_tq_profiles.md)。

## 当前内容

```text
include/flash_model/   公共头文件
src/config/            配置结构、加载器、验证器
src/core/              Common Core：地址映射、寄存器、时序、存储后端
src/capabilities/      Capability Modules：可选能力模块及 hook
src/policies/          Device Policy：通用和厂商族策略
src/model/             FlashModel 命令级外观层
src/builder/           Config -> Policy/Capability -> FlashModel 组装入口
src/cli/               命令行 demo 和 smoke 自测入口
configs/               分层配置样例
schema/                配置 schema 草案
docs/                  能力边界和能力矩阵文档
Makefile               C++17 构建入口
```

每个代码目录都带有自己的 `README.md`，用于说明该层职责、文件含义和新增代码的落点。

## 命名约定

目录表达层次，文件名表达职责，因此代码文件尽量使用短名：

- `src/core/address.cpp`
- `src/core/timing.cpp`
- `src/capabilities/capability.cpp`
- `src/model/model.cpp`
- `src/builder/builder.cpp`

类型名保持语义清楚，例如 `FlashModel`、`AddressMapper`、`CapabilityModule`。

## 构建和运行

```bash
cd /home/wsl/project/FLASH_MOD/FLASH_MODLE
make
make run-nor
make run-nand
make test
```

## 当前定位

当前版本先实现框架形态和最小公共行为路径，不追求一次性覆盖所有旧模型的厂商私有行为。

## 当前验证

`make test` 会覆盖：

- NOR demo 自测。
- NOR page program wrap 行为：跨 256B 页尾写入会在同一页内回绕。
- NOR `nor_block_erase` / `nor_chip_erase` 行为：按 block 或全片恢复 `0xFF`。
- NOR `deep_power_down` 行为：低功耗期间普通读写擦被拒绝，release 后恢复。
- NOR `security_register` 行为：3 个 256B security register 支持 erase/program/read 和 `old & new`。
- NOR `block_protect` 行为：保护区内 program/erase 应失败，未保护区读写擦仍应通过。
- SPI-NAND demo 自测。
- SPI-NAND `block_protect` 行为：默认保护时擦除应失败，解保护后再擦除。
- SPI-NAND `ecc_status` 行为：写入 ECC reserved spare 区后 program execute 应失败。
- SPI-NAND `bad_block_management` 行为：配置的 initial bad block 不允许 erase/program。
- SPI-NAND `copy_back` 行为：源页数据可经内部 cache copy 到目标页，并遵守 program 约束。
- SPI-NAND `strict_sequential_program` 行为：跳页编程应失败。
- `AddressMapper` 单元式测试：NOR sector 对齐、保护范围 overlap、NAND page/block offset。
- `RegisterEngine` 单元式测试：WEL 位、Feature C0h 动态状态、P_FAIL/E_FAIL 合成。
- `TimingEngine` 单元式测试：advance、busy 自动完成、wait-ready、clear-busy。
- `CapabilityModule` 单元式测试：`block_protect` hook 拦截 NOR 保护窗口和 SPI-NAND A0h 全保护。
- `CapabilityModule` 单元式测试：`ecc_status` hook 标记 ECC reserved spare 区，并在 program execute 前拒绝提交。
- `CapabilityModule` 单元式测试：`bad_block_management` hook 挂载 initial bad block，并拒绝坏块 erase/program。
- `CapabilityModule` 单元式测试：`copy_back` hook 拒绝同页 copy-back，并允许跨页 copy-back。
- `Evidence Validator` 单元式测试：demo 配置具备字段级 evidence，缺失 evidence 的配置会产生 evidence gap。
- 负例配置验证：NOR 配置启用 NAND-only 命令应被 validator 拦截。
- datasheet profile 验证：`by25q64as.yaml`、`w25q32jv.yaml`、`w25n01gv.yaml`、`mt29f2g01.yaml` 均可通过 `--validate-only`。

后续应把已有单芯片模型中的特殊行为逐步沉淀到：

- `capability module`
- `device policy`
- `schema` 字段
- `golden behavior test`
## 2026-06 Multi-Part Iteration

This iteration extends the unified framework in several layers at once:

- Config/schema: added explicit fields for address width, security register
  sizing, OTP page count, read-retry levels, and die count.
- Loader/validator: parses the new fields and checks command/capability
  consistency for NOR and NAND profiles.
- Core/model: added suspend/resume timing, configurable security registers,
  security lock, NAND OTP storage, 4-byte address mode state, die/plane select,
  and read-retry state.
- Command transaction layer: records opcode values, dummy cycles, phase lane
  metadata, and command aliases while preserving the existing byte-based timing
  path.
- Builder: can now build from either a config file or an in-memory `ModelConfig`.
- Tests: added `model_feature_test.cpp` and expanded smoke coverage.

The project is still following the original target shape:

```text
External AI -> Structured Config + Evidence -> Validator -> Builder
            -> Common Core + Capability Modules + Device Policy -> FlashModel
```
