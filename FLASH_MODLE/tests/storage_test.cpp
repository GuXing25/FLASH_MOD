#include "flash_model/storage.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace flash_model;

int main()
{
    // 新建 4 字节阵列，未写入区域应全部读出擦除态 0xFF。
    SparseStorageBackend storage(4);
    assert((storage.read(0, 4) == std::vector<std::uint8_t>{0xFF, 0xFF, 0xFF, 0xFF}));

    // program 使用 old & new，因此第二次写入只能继续把 1 拉成 0。
    storage.program_and(1, {0x0F});
    storage.program_and(1, {0xF0});
    assert((storage.read(0, 4) == std::vector<std::uint8_t>{0xFF, 0x00, 0xFF, 0xFF}));

    // erase 删除稀疏 map 中的非擦除态字节，读回再次变成 0xFF。
    storage.erase(1, 1);
    assert((storage.read(0, 4) == std::vector<std::uint8_t>{0xFF, 0xFF, 0xFF, 0xFF}));

    // wrap 模式允许跨过末尾回到开头，符合部分 NOR page wrap/线性回绕场景。
    storage.program_and(3, {0xAA, 0x55});
    assert((storage.read(0, 4) == std::vector<std::uint8_t>{0x55, 0xFF, 0xFF, 0xAA}));

    // 极大地址在 wrap 模式下也按数学取模处理，不依赖 uint64_t 自然溢出。
    const std::uint64_t max = std::numeric_limits<std::uint64_t>::max();
    storage.erase(0, 4);
    storage.program_and(max, {0x00, 0x00});
    assert((storage.read(0, 4) == std::vector<std::uint8_t>{0x00, 0xFF, 0xFF, 0x00}));

    // 非 wrap 模式下，越界或加法溢出必须抛出异常，不能悄悄回绕。
    bool threw = false;
    try {
        (void)storage.read(max, 2, false);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    assert(threw);

    std::cout << "StorageBackend tests PASS\n";
    return 0;
}
