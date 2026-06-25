#ifndef FLASH_MODEL_STORAGE_BACKEND_HPP
#define FLASH_MODEL_STORAGE_BACKEND_HPP

#include <cstdint>
#include <map>
#include <vector>

namespace flash_model {

// SparseStorageBackend 是面向功能仿真的稀疏阵列后端。
//
// 未写入的位置默认读出 0xFF，因此小 demo 不需要分配完整 1Gb/2Gb NAND 镜像。
// program_and() 使用 old & new 语义，模拟 Flash 只能从 1 编程为 0。
// erase() 删除稀疏 map 中的字节，让对应范围重新回到擦除态 0xFF。
class SparseStorageBackend {
public:
    explicit SparseStorageBackend(std::uint64_t size_bytes = 0);

    std::uint64_t size() const { return size_bytes_; }
    void resize(std::uint64_t size_bytes);

    std::vector<std::uint8_t> read(std::uint64_t address, std::size_t length, bool wrap = true) const;
    void program_and(std::uint64_t address, const std::vector<std::uint8_t>& data, bool wrap = true);
    void erase(std::uint64_t address, std::uint64_t length, bool wrap = true);

private:
    std::uint64_t size_bytes_ = 0;
    std::map<std::uint64_t, std::uint8_t> bytes_;

    std::uint64_t normalize(std::uint64_t address, bool wrap) const;
    void erase_linear(std::uint64_t address, std::uint64_t length);
};

} // namespace flash_model

#endif
