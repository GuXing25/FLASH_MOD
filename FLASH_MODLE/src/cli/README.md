# src/cli

本目录放命令行入口和 demo 自测流程。CLI 是验证和演示入口，不是模型逻辑本体。

## 文件说明

- `main.cpp`：加载配置，打印模型 profile，运行 NOR/SPI-NAND self-test。

## 当前命令

```bash
./flash_model_sim configs/demo_nor.yaml --self-test
./flash_model_sim configs/demo_nand.yaml --self-test
./flash_model_sim configs/nor_w25q32jv.yaml --validate-only
./flash_model_sim configs/demo_nand.yaml --self-test --storage-dir out/storage
```

## 存储区导出

当前模型运行时仍使用内存稀疏存储后端。需要留下模拟存储区时，可以在命令后追加：

```bash
./flash_model_sim configs/demo_nor.yaml --self-test --dump-storage out/storage/demo_nor.bin
./flash_model_sim configs/demo_nand.yaml --self-test --storage-dir out/storage
./flash_model_sim configs/demo_nand.yaml --self-test --dump-otp out/storage/demo_nand_otp.bin
```

`--dump-storage` 导出主阵列镜像，`--dump-otp` 导出 NAND OTP 区镜像，`--storage-dir` 会按器件名自动生成 `<device>_array.bin`，并在存在 OTP 区时生成 `<device>_otp.bin`。同时会生成 `<device>_manifest.txt`，记录配置路径、器件信息、镜像大小和仿真结束时间。`--dump-dir` 是兼容旧写法的别名。

镜像大小和线性布局由配置文件决定：

- NOR：`array_size = geometry.memory_size`，线性布局为 `offset = byte_address`。
- NAND：`array_size = blocks * pages_per_block * (main_size + spare_size)`，线性布局为 `offset = ((block * pages_per_block) + page) * page_size + column`。
- OTP：`otp_size = otp_page_count * page_size`。

`make clean` 会删除 `out/`，包括通过 `make dump-nor` / `make dump-nand` 生成的模拟存储区。

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
