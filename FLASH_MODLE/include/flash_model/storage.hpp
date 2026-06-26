#ifndef FLASH_MODEL_STORAGE_HPP
#define FLASH_MODEL_STORAGE_HPP

#include <cstdint>
#include <map>
#include <vector>

namespace flash_model {

// SparseStorageBackend 是稀疏阵列存储后端。
// 未写过的地址默认为擦除态 0xFF；program 使用 old & new，模拟 Flash 只能 1->0。
class SparseStorageBackend {
public:
    explicit SparseStorageBackend(std::uint64_t size_bytes = 0);

    std::uint64_t size() const { return size_bytes_; }
    void resize(std::uint64_t size_bytes);

    // read 支持 wrap，便于模拟部分 NOR page wrap 或线性地址回绕场景。
    std::vector<std::uint8_t> read(std::uint64_t address, std::size_t length,
                                   bool wrap = true) const;
    // program_and 不会把 0 写回 1；擦除必须走 erase。
    void program_and(std::uint64_t address, const std::vector<std::uint8_t>& data,
                     bool wrap = true);
    // erase 把目标范围恢复为默认 0xFF，因此稀疏 map 中对应项会被删除。
    void erase(std::uint64_t address, std::uint64_t length, bool wrap = true);

private:
    std::uint64_t size_bytes_ = 0; // 阵列总容量。
    std::map<std::uint64_t, std::uint8_t> bytes_; // 只记录非 0xFF 的字节。

    std::uint64_t normalize(std::uint64_t address, bool wrap) const;
    std::uint64_t normalize_offset(std::uint64_t address,
                                   std::uint64_t offset,
                                   bool wrap) const;
    void erase_linear(std::uint64_t address, std::uint64_t length);
};

} // namespace flash_model

#endif
