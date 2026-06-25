# FLASH_MODLE

`FLASH_MODLE` 是下一代统一 Flash 模型框架实验目录。

它验证的核心方向是：

```text
Common Core + Capability Modules + Device Policy
```

详细路线见 [总路线.md](总路线.md)。

## 当前内容

```text
include/flash_model/   公共头文件
src/                   框架实现
configs/               分层配置样例
schema/                配置 schema 草案
docs/                  能力边界和能力矩阵文档
Makefile               C++17 构建入口
```

## 构建和运行

```bash
cd /home/wsl/project/FLASH_MODLE
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
- NOR `block_protect` 行为：保护区内 program/erase 应失败，未保护区读写擦仍应通过。
- SPI-NAND demo 自测。
- SPI-NAND `block_protect` 行为：默认保护时擦除应失败，解保护后再擦除。
- SPI-NAND `ecc_status` 行为：写入 ECC reserved spare 区后 program execute 应失败。
- SPI-NAND `strict_sequential_program` 行为：跳页编程应失败。
- `AddressMapper` 单元式测试：NOR sector 对齐、保护范围 overlap、NAND page/block offset。
- `RegisterEngine` 单元式测试：WEL 位、Feature C0h 动态状态、P_FAIL/E_FAIL 合成。
- `TimingEngine` 单元式测试：advance、busy 自动完成、wait-ready、clear-busy。
- 负例配置验证：NOR 配置启用 NAND-only 命令应被 validator 拦截。

后续应把已有单芯片模型中的特殊行为逐步沉淀到：

- `capability module`
- `device policy`
- `schema` 字段
- `golden behavior test`
