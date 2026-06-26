# falsh_flash

本文档用于记录当前 `FLASH_MODLE` 项目的进程、总体路线、模型方案、软件架构、当前已完成能力、剩余缺口和后续推进计划。

项目当前目标不是为每一颗 Flash 单独手写一个完整模型，而是形成一个 **统一的行为级 Flash 模型框架**：

```text
Datasheet / 外部 AI
        |
        v
结构化配置 + source evidence
        |
        v
Config Loader / Validator
        |
        v
Model Builder
        |
        v
FlashModel
  |
  +-- Common Core
  +-- Chiplet Interface
  +-- Command Transaction Layer
  +-- Command Facade
  +-- Capability Modules
  +-- Device Policy
```

核心思想是：**外部 AI 从数据手册中提取必要参数并生成结构化配置；本项目的统一框架读取配置，组合出对应器件的 Flash 行为级模型。**

---

## 1. 项目定位

本项目关注的是 **行为级 Flash 模型**，不是晶体管级、电气级、引脚级模型。

当前模型重点关注：

- 存储体行为
- 地址映射
- 读、写、擦除等操作行为
- 操作伴随的 busy/wait-ready 时序
- 操作耗时推进
- 数据交换
- 状态寄存器和 feature register
- NOR / SPI-NAND 的共同行为和差异行为
- 芯粒与芯粒之间的抽象 transaction 接口

当前模型暂不重点模拟：

- 真实电压、电流、温度漂移等物理层行为
- 引脚电气特性
- 信号完整性
- PHY 训练
- cycle-accurate 总线波形
- NAND 内部真实 ECC 算法
- 真实磨损、retention、disturb 等可靠性退化机制

这些内容可以在后续需要时作为更高保真层扩展，但不应污染当前行为级统一框架。

---

## 2. 总体路线

项目路线可以概括为：

```text
配置标准化
  -> 配置验证器强化
  -> Common Core 稳定
  -> 命令事务化
  -> 可选能力模块化
  -> 厂商差异 policy 化
  -> datasheet profile + golden test
  -> 外部 AI 自动生成配置
```

更具体地说：

1. 先把数据手册中可结构化的信息沉淀到统一 schema 中。
2. 再由 loader/validator 确保配置合法、完整、可追溯。
3. Common Core 只承载 NOR/NAND 共享的稳定底层行为。
4. Command Facade 负责对外提供行为级命令入口。
5. Command Transaction Layer 负责描述命令的数据交换形态。
6. Chiplet Interface 负责把数据交换转换为芯粒间传输耗时。
7. Capability Modules 负责可选能力。
8. Device Policy 只处理少量难以配置化、模块化的厂商差异。
9. 每个 datasheet profile 都需要逐渐配套 golden behavior test。

---

## 3. 总体方案

最初路线中提出的方案是：

```text
统一内核 + 可配置能力模块 + 器件策略层
```

现在项目仍然按照这个方案推进。

### 3.1 纯参数差异

适合放入配置文件。

例如：

- 器件名称
- 厂商
- part number
- JEDEC ID
- 容量
- page size
- sector size
- block size
- NAND main/spare size
- pages per block
- blocks
- dies/planes
- read/program/erase/reset timing
- 默认状态寄存器值
- feature register 默认值
- OTP page 数量
- security register 数量和大小
- unique ID 长度
- SFDP/parameter page 大小

### 3.2 可选能力差异

适合做成 capability module。

例如：

- block protect
- security register
- OTP
- ECC reserved range
- ECC status
- bad block management
- copy-back
- unique ID
- SFDP
- parameter page
- deep power-down
- suspend/resume
- 4-byte address mode
- die select
- plane select
- read retry

### 3.3 厂商私有规则差异

适合放在 policy 层，而不是塞进 Common Core。

例如：

- Winbond family 的某些 status/feature 位解释
- Micron family 的部分 NAND feature 习惯
- GigaDevice family 的 parameter page / ECC 行为差异
- Macronix / Boya 的 NOR 保护、security register 差异
- 某些厂商特有的 lock bit 编码
- 某些厂商特定的 read-retry 流程

---

## 4. 当前软件架构

当前代码已经按分层方式组织。

```text
FLASH_MODLE
  |
  +-- include/flash_model
  |     +-- config.hpp
  |     +-- loader.hpp
  |     +-- validator.hpp
  |     +-- builder.hpp
  |     +-- model.hpp
  |     +-- address.hpp
  |     +-- command.hpp
  |     +-- interface.hpp
  |     +-- registers.hpp
  |     +-- storage.hpp
  |     +-- timing.hpp
  |     +-- capability.hpp
  |     +-- policy.hpp
  |
  +-- src/config
  |     +-- config.cpp
  |     +-- loader.cpp
  |     +-- validator.cpp
  |
  +-- src/core
  |     +-- address.cpp
  |     +-- command.cpp
  |     +-- interface.cpp
  |     +-- registers.cpp
  |     +-- storage.cpp
  |     +-- timing.cpp
  |
  +-- src/model
  |     +-- model.cpp
  |
  +-- src/capabilities
  |     +-- capability.cpp
  |
  +-- src/policies
  |     +-- policy.cpp
  |
  +-- src/builder
  |     +-- builder.cpp
  |
  +-- src/cli
  |     +-- main.cpp
  |
  +-- configs
  |     +-- demo profiles
  |     +-- datasheet profiles
  |
  +-- schema
  |     +-- flash_model_schema.yaml
  |
  +-- tests
  |     +-- unit tests
  |     +-- smoke tests
  |
  +-- docs
        +-- route / capability / interface / transaction documents
```

---

## 5. 配置层

配置层是整个项目的入口。

它的目标是让外部 AI 不生成 C++，而是生成统一结构的配置文件。

当前 schema 包括：

- `device`
- `geometry`
- `timing`
- `interface`
- `commands`
- `registers`
- `capabilities`
- `constraints`
- `transactions`
- `policy`
- `source`
- `evidence`
- `unsupported_features`

### 5.1 device

描述器件身份。

典型字段：

- `name`
- `class`
- `manufacturer`
- `part_number`
- `id`
- `unique_id`

### 5.2 geometry

描述存储体几何结构。

NOR 典型字段：

- `memory_size`
- `page_size`
- `sector_size`
- `block32_size`
- `block_size`

NAND 典型字段：

- `main_size`
- `spare_size`
- `pages_per_block`
- `blocks`
- `planes`
- `dies`

### 5.3 timing

描述器件侧操作耗时。

当前字段包括：

- `read_us`
- `page_read_us`
- `program_us`
- `sector_erase_us`
- `block32_erase_us`
- `block_erase_us`
- `chip_erase_us`
- `reset_us`
- `puw_us`

这些时序属于器件行为本身，不属于芯粒接口传输时间。

### 5.4 interface

描述芯粒与芯粒之间的抽象 transaction 接口。

当前字段包括：

- `enabled`
- `name`
- `protocol`
- `lanes`
- `data_width_bits`
- `clock_mhz`
- `fixed_latency_us`
- `turnaround_us`
- `packet_overhead_bytes`
- `max_payload_bytes`

当前只是暂定的 `generic_chiplet` 行为级接口模型。

它不模拟引脚、电气、PHY，只把一次命令事务的数据交换换算为传输耗时。

### 5.5 commands

描述该器件支持哪些行为级命令。

例如：

- `read_id`
- `reset`
- `nor_read`
- `nor_program`
- `nor_erase`
- `nor_block32_erase`
- `nor_block_erase`
- `nor_chip_erase`
- `read_unique_id`
- `read_sfdp`
- `deep_power_down`
- `release_power_down`
- `suspend`
- `resume`
- `page_read`
- `read_from_cache`
- `program_load`
- `program_execute`
- `block_erase`
- `copy_back`
- `read_parameter_page`

### 5.6 capabilities

描述该器件具备哪些可选能力。

例如：

- `quad_enable`
- `four_byte_address`
- `block_protect`
- `deep_power_down`
- `suspend_resume`
- `unique_id`
- `sfdp`
- `otp`
- `security_register`
- `ecc_status`
- `bad_block_management`
- `copy_back`
- `die_select`
- `plane_select`
- `read_retry`
- `parameter_page`

### 5.7 constraints

描述命令执行约束或能力参数。

例如：

- `require_wel`
- `strict_sequential_program`
- `max_partial_programs`
- `ecc_reserved_start`
- `ecc_reserved_end`
- `initial_bad_blocks`
- `nor_protect_start`
- `nor_protect_length`
- `address_bytes`
- `security_register_count`
- `security_register_size`
- `otp_page_count`
- `read_retry_levels`
- `unique_id_size`
- `sfdp_size`
- `parameter_page_size`
- `bad_block_marker_offset`
- `minimum_valid_blocks`
- `ecc_bits`

### 5.8 transactions

这是最近新增的重要结构。

它描述一条命令在芯粒接口上的数据交换形态，而不是描述命令的存储行为。

典型字段：

- `opcode_bytes`
- `address_bytes`
- `dummy_bytes`
- `fixed_request_bytes`
- `fixed_response_bytes`
- `use_current_address_bytes`
- `write_payload`
- `read_response`
- `turnaround`

例如：

```yaml
transactions:
  nor_read:
    opcode_bytes: 1
    address_bytes: 0
    dummy_bytes: 0
    fixed_request_bytes: 0
    fixed_response_bytes: 0
    use_current_address_bytes: true
    write_payload: false
    read_response: true
    turnaround: true
```

这表示：

- `nor_read` 有 1 字节 opcode
- 地址字节数使用当前地址模式，比如 3-byte 或 4-byte
- 没有 dummy byte
- 不带写 payload
- 读返回数据
- 需要 request-to-response turnaround

---

## 6. Config Loader / Validator

### 6.1 Loader

Loader 负责把 YAML-like 配置读入 `ModelConfig`。

当前支持：

- 基本字段读取
- 十六进制 ID 读取
- list 字段读取
- source/evidence 读取
- policy overrides
- transactions 子段读取

### 6.2 Validator

Validator 负责在模型构建前检查配置合法性。

当前会检查：

- schema version
- device name
- class 合法性
- NOR/NAND geometry 必需字段
- NOR/NAND 命令混用
- NOR/NAND capability 混用
- timing 缺失 warning
- policy name
- evidence 缺失
- WEL/feature/capability 组合
- ECC reserved range
- bad block 范围
- security register 参数
- unique ID/SFDP/parameter page 参数
- chiplet interface 参数
- transactions 参数

Validator 是后续外部 AI 配置生成链路中非常关键的安全网。

---

## 7. Common Core

Common Core 只存放 NOR/NAND 共享且稳定的基础行为。

当前 Common Core 包括：

```text
StorageBackend
AddressMapper
TimingEngine
RegisterEngine
InterfaceEngine
Command Transaction Layer
```

### 7.1 StorageBackend

当前存储后端是稀疏存储模型。

语义：

- 默认擦除态为 `0xFF`
- 擦除操作把范围恢复为 `0xFF`
- 编程操作采用 `old & new`
- 因此只能模拟 `1 -> 0`
- 不允许通过 program 把 `0 -> 1`

这符合 Flash 的基本行为级语义。

### 7.2 AddressMapper

当前支持：

- NOR sector base
- NOR 32KiB block base
- NOR block base
- NAND page offset
- NAND block offset
- NAND block/page 转换

### 7.3 TimingEngine

当前支持：

- 当前仿真时间 `now_us`
- `advance`
- `start_busy`
- `busy`
- `wait_ready`
- `suspend`
- `resume`
- `clear_busy`

核心模型是单 outstanding busy window。

后续如果需要更复杂并发，可以扩展为多 outstanding operation 或队列化 timing。

### 7.4 RegisterEngine

当前支持：

- WEL
- status1/status2/status3
- feature A0/B0/C0/D0
- dynamic C0
- program fail
- erase fail
- quad enable state
- address byte mode
- OTP mode
- active die/plane
- read retry level

### 7.5 InterfaceEngine

InterfaceEngine 是暂定的芯粒间 transaction 计时模型。

公式：

```text
payload_bytes = request_bytes + response_bytes
packet_count = ceil(payload_bytes / max_payload_bytes)
transfer_bytes = payload_bytes + packet_count * packet_overhead_bytes
bandwidth_bytes_per_us = clock_mhz * lanes * data_width_bits / 8
interface_time_us =
    fixed_latency_us
  + optional turnaround_us
  + transfer_bytes / bandwidth_bytes_per_us
```

如果 `interface.enabled = false`，接口传输耗时为 0，老配置保持原行为。

### 7.6 Command Transaction Layer

这是当前最新补充的核心层。

它把命令的数据交换形态从 `FlashModel` 中拆出来。

当前链路：

```text
FlashModel command
  -> command name + dynamic payload/response length
  -> TransactionConfig
  -> CommandTransfer
  -> InterfaceEngine
  -> TimingEngine
```

这样以后某条命令的传输形态变化时，不需要改命令行为逻辑。

例如：

- `nor_read`: opcode + current address width + read response
- `nor_program`: opcode + current address width + write payload
- `program_load`: opcode + 2-byte column + write payload
- `read_from_cache`: opcode + column + dummy + read response
- `get_feature`: opcode + feature address + 1-byte response

---

## 8. Command Facade

`FlashModel` 是当前模型的命令级外观层。

它负责串起：

- config
- storage
- address
- timing
- registers
- interface
- command transaction
- capabilities
- policy

当前支持的 NOR 行为包括：

- read ID
- read unique ID
- read SFDP
- normal read
- page program
- sector erase
- 32KiB block erase
- block erase
- chip erase
- write enable / write disable
- reset
- deep power-down / release
- enter/exit 4-byte address mode
- suspend/resume
- security register read/program/erase/lock

当前支持的 SPI-NAND 行为包括：

- read ID
- read unique ID
- get feature
- set feature
- page read
- read from cache
- program load
- program execute
- block erase
- copy-back
- enter/exit OTP mode
- read parameter page
- die select
- plane select
- read retry level

---

## 9. Capability Modules

Capability Modules 用来表达可选功能。

当前已经具备的能力模块/能力逻辑包括：

- `block_protect`
- `ecc_status`
- `bad_block_management`
- `copy_back`
- `quad_enable`
- `four_byte_address`
- `deep_power_down`
- `suspend_resume`
- `unique_id`
- `sfdp`
- `otp`
- `security_register`
- `die_select`
- `plane_select`
- `read_retry`
- `parameter_page`

其中部分能力已经具备真实行为 hook，例如：

- block protect 阻止受保护区域 program/erase
- ECC reserved range 阻止非法 program load
- bad block 阻止 bad block program/erase
- copy-back 检查目标块

部分能力目前主要是配置、校验和状态入口，后续仍需增强厂商细节。

---

## 10. Device Policy

Policy 层用于少量厂商差异。

当前方向：

```text
generic_nor
generic_spinand
winbond_family
micron_family
gigadevice_family
macronix_family
boya_family
```

Policy 不应该退化成“每颗芯片一个完整模型”。

它只应该处理：

- 很难参数化的厂商规则
- 很难模块化的 family 差异
- 少量 power-on/reset 默认行为
- 少量 status/feature 位解释差异
- 特殊保护策略

---

## 11. 当前 datasheet profile 进度

当前项目已经根据已给出的提取信息和部分数据手册形成多份 profile。

当前已有 NOR profile 包括：

- `w25q32jv.yaml`
- `by25q64as.yaml`
- `mx25l25645g.yaml`
- `m25p40.yaml`
- `gd25le128e.yaml`

当前已有 SPI-NAND profile 包括：

- `w25n01gv.yaml`
- `mt29f2g01.yaml`
- `gd5f1gm7ue.yaml`

当前还有 demo profile：

- `demo_nor.yaml`
- `demo_nor_block_protect.yaml`
- `demo_spinand.yaml`

这些 profile 当前主要覆盖：

- identity
- geometry
- basic timing
- command support
- capability flags
- basic constraints
- source/evidence
- 部分 SFDP/parameter page/OTP/security/bad block/ECC 相关参数

---

## 12. 当前测试体系

当前测试包括：

- `address_test`
- `registers_test`
- `timing_test`
- `capability_test`
- `evidence_test`
- `interface_test`
- `command_test`
- `model_feature_test`
- `smoke.sh`

当前完整验证命令：

```bash
make test
make clean
```

最近一次验证已经通过：

```text
AddressMapper tests PASS
RegisterEngine tests PASS
TimingEngine tests PASS
CapabilityModule tests PASS
Evidence validator tests PASS
Interface tests PASS
Command transaction tests PASS
Model feature tests PASS
FLASH_MODLE smoke tests PASS
```

---

## 13. 当前完成到了哪一步

按最初路线来看，当前已经完成：

### 已完成

- 项目目录分层
- README/文档体系初步建立
- 统一配置结构
- loader
- validator
- builder
- Common Core 初版
- StorageBackend
- AddressMapper
- TimingEngine
- RegisterEngine
- Chiplet Interface 初版
- Command Transaction Layer 初版
- FlashModel command facade
- NOR 基础行为
- SPI-NAND 基础行为
- 部分 capability modules
- 部分 vendor family policy
- 多个 datasheet profile
- source/evidence 机制
- 单元测试和 smoke test

### 部分完成

- SFDP：当前有基础 SFDP header 和少量字段，未完整生成全表。
- parameter page：当前可生成基础 ONFI-like parameter page，未完全覆盖所有 vendor 细节。
- OTP：当前有基本 OTP page/storage 语义，锁定规则和 vendor map 还不完整。
- security register：当前有 read/program/erase/lock 基础行为，vendor lock bit 编码还不完整。
- block protect：当前支持配置范围保护，未完整解析所有 NOR BP/TB/CMP/SEC 组合。
- ECC：当前支持 ECC reserved range 和 status 基础位，未模拟纠错强度细节。
- read retry：当前有状态入口，完整流程未建模。
- die/plane select：当前有状态入口，地址映射细节仍需增强。
- chiplet interface：当前有 transaction-level timing，未建模真实协议队列、QoS、重试等。

---

## 14. 当前还缺什么

### 14.1 配置 schema 还需要继续细化

尤其是：

- command opcode 值
- dummy cycle 与 dummy byte 的区分
- timing min/typ/max
- timing 条件，比如电压、温度、模式
- command alias
- vendor-specific register bit maps
- protection table
- OTP region map
- security region map
- NAND parameter page 字段来源
- SFDP table 字段来源

### 14.2 Command Transaction Layer 还需要增强

当前只记录：

- opcode bytes
- address bytes
- dummy bytes
- request/response bytes
- payload direction
- turnaround

后续可能需要：

- opcode value
- bus mode / lane mode
- command/address/data 分阶段描述
- dummy cycles
- alternate bytes
- command alias
- continuous read mode
- cache pipeline
- multi-plane command sequence
- multi-die routing

### 14.3 Chiplet Interface 还只是初版

当前只支持：

- lanes
- width
- clock
- fixed latency
- turnaround
- packet overhead
- max payload fragmentation

后续真实接口声明确定后，需要考虑：

- queue depth
- multiple outstanding transactions
- arbitration
- QoS
- credit/backpressure
- retry/NAK/CRC error
- virtual channels
- ordering rules
- command/data response 分离
- streaming / burst
- link power state

### 14.4 Capability Modules 还需要继续下沉逻辑

当前有些行为仍在 `model.cpp` 内。

后续应逐步把下面内容拆到 capability module：

- OTP lock / OTP page map
- security register lock policy
- read retry 流程
- plane select 地址映射
- die select 地址映射
- advanced bad block marker scan
- BBM LUT / link table
- ECC status interpretation
- NOR protection table decoding

### 14.5 Device Policy 需要更多真实 datasheet 驱动

当前 family policy 还只是基础入口。

后续需要根据数据手册逐步补：

- Winbond NOR/NAND
- Micron SPI-NAND
- GigaDevice SPI-NAND/NOR
- Macronix NOR
- Boya NOR

Policy 必须保持“少量差异入口”的定位。

### 14.6 Golden Test 不够细

当前测试偏通用。

后续每个 profile 应有更细的 golden test：

- ID 是否正确
- geometry 是否正确
- read/program/erase 是否正确
- busy timing 是否符合配置
- WEL/WIP/OIP/status 是否正确
- feature register 默认值和动态位是否正确
- protection 行为是否正确
- OTP/security 行为是否正确
- bad block 行为是否正确
- parameter page/SFDP 输出是否正确
- command transaction timing 是否正确

---

## 15. 后续推进计划

建议后续按以下顺序推进。

### 阶段 1：稳定 schema 和 profile

目标：

- 让外部 AI 知道应该提取什么
- 让配置文件成为真正的模型输入接口

任务：

- 完善 schema 字段说明
- 为每个字段明确单位
- 为每个字段明确适用 NOR/NAND
- 为每个字段明确是否需要 evidence
- 为已有 profile 补齐缺失 evidence
- 整理 unsupported features

### 阶段 2：加强命令描述

目标：

- 让命令行为和数据交换彻底解耦

任务：

- 在 `transactions` 中加入 opcode value
- 支持 dummy cycle
- 支持 command alias
- 支持 command/address/data phase
- 支持不同 bus width
- 支持 continuous/cache pipeline 描述

### 阶段 3：加强 chiplet interface

目标：

- 使模型可以表达芯粒间接口开销，而不是简单 SPI byte count

任务：

- 等待真实接口声明
- 根据真实声明补字段
- 增加队列/并发/仲裁
- 增加 backpressure
- 增加 virtual channel
- 增加 request/response 分离
- 增加 transaction trace 或统计接口

### 阶段 4：能力模块继续拆分

目标：

- 让 `model.cpp` 更薄
- 让可选能力独立演进

任务：

- OTP module 完整化
- security register module 完整化
- block protect table module
- read retry module
- die/plane mapping module
- parameter page module
- SFDP module
- BBM module

### 阶段 5：Policy 由 datasheet 驱动增强

目标：

- 保持统一框架，同时容纳少量厂商差异

任务：

- 按 family 梳理差异表
- 只把不能配置化的行为放入 policy
- 为每个 policy 建单独测试
- 避免 policy 退化成单芯片模型

### 阶段 6：profile golden test

目标：

- 每个数据手册 profile 都有可信回归测试

任务：

- 每个 profile 至少一个 validate-only test
- 每个 profile 至少一个 ID/geometry test
- NOR profile 增加 program/erase/read test
- NAND profile 增加 page read/cache/program/block erase test
- 有能力的 profile 增加 OTP/security/bad block/copy-back test
- 对 parameter page/SFDP 做字段级检查

### 阶段 7：外部 AI 配置生成流程

目标：

- 外部 AI 可以稳定生成配置文件
- 本项目可以自动验证并生成模型

任务：

- 提供 schema 说明文档
- 提供字段提取 prompt 或 checklist
- 要求每个字段带 evidence
- 要求 evidence 包括 datasheet、page、confidence
- 加入配置质量报告
- 对低置信度字段做 warning

---

## 16. 外部 AI 应如何配合

外部 AI 的职责应该是：

```text
读取 datasheet
  -> 提取字段
  -> 生成结构化配置
  -> 给每个关键字段附 evidence
  -> 标记不确定字段
  -> 标记 unsupported features
```

外部 AI 不应该：

- 生成 C++ 模型代码
- 手写存储行为
- 手写时序引擎
- 任意创造字段
- 把不能确认的信息当作确定值

推荐输出内容：

```yaml
device:
  name: ...
  class: ...
  manufacturer: ...
  id: [...]

geometry:
  ...

timing:
  ...

commands:
  ...

capabilities:
  ...

constraints:
  ...

source:
  datasheet: ...
  page: ...
  confidence: ...

evidence:
  geometry.memory_size: "datasheet|page|confidence"
```

---

## 17. 当前路线的核心取舍

本项目不追求“一个万能配置文件解释所有 datasheet 行为”。

更合理的比例是：

```text
80% 通用行为：配置化
15% 可选能力：模块化
5% 厂商特例：policy 化
```

这样可以避免模型变成复杂、难验证、难维护的解释器。

当前架构的优势是：

- 统一
- 可扩展
- 可测试
- 可追溯
- 能承接外部 AI 生成配置
- 不需要每颗芯片单独写模型
- 能逐步提高保真度

---

## 18. 当前最重要的下一步

从现在状态看，最值得继续做的事情是：

1. **继续增强 Command Transaction Layer**
   - 加 opcode value
   - 加 dummy cycle
   - 加 bus phase
   - 加 command alias

2. **把 SFDP / parameter page 从 model.cpp 中进一步模块化**
   - 建立 `sfdp` capability/module
   - 建立 `parameter_page` capability/module
   - 从配置生成更完整的数据页

3. **为已有 datasheet profile 建 golden tests**
   - 不只 validate-only
   - 要真正跑读写擦除、feature、parameter page、security/OTP 等行为

4. **继续根据数据手册补 capability 和 policy**
   - 尤其是 protection、OTP/security、ECC、bad block、read retry

5. **等待或整理 chiplet interface 正式声明**
   - 当前 interface 是暂定 transaction 模型
   - 正式声明出来后再扩展 queue/backpressure/QoS 等

---

## 19. 简短结论

当前项目已经从“多个独立 Flash 模型”推进到了“统一行为级 Flash 模型框架”的阶段。

现在已经具备：

- 统一配置入口
- 配置校验
- 模型构建器
- Common Core
- FlashModel 命令 facade
- Capability Modules
- Device Policy
- Chiplet Interface 初版
- Command Transaction Layer 初版
- 多个 datasheet profile
- 单元测试和 smoke test

后续重点不是推翻架构，而是沿着当前架构继续补齐：

```text
更完整的配置 schema
更强的命令事务描述
更真实的能力模块
更少但更准确的厂商 policy
更细的 profile golden test
更明确的 chiplet interface 真实协议
```

最终目标仍然是：

```text
外部 AI 提取 datasheet 参数
        |
        v
生成结构化配置 + evidence
        |
        v
统一 Flash 行为级模型框架加载配置
        |
        v
自动组合出对应 Flash 模型
```

