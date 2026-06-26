#ifndef FLASH_MODEL_STORAGE_HPP
#define FLASH_MODEL_STORAGE_HPP

#include <cstdint>
#include <map>
#include <vector>

namespace flash_model {

// Sparse array storage with erased 0xFF default and 1->0 program semantics.
class SparseStorageBackend {
public:
    explicit SparseStorageBackend(std::uint64_t size_bytes = 0);

    std::uint64_t size() const { return size_bytes_; }
    void resize(std::uint64_t size_bytes);

    std::vector<std::uint8_t> read(std::uint64_t address, std::size_t length,
                                   bool wrap = true) const;
    void program_and(std::uint64_t address, const std::vector<std::uint8_t>& data,
                     bool wrap = true);
    void erase(std::uint64_t address, std::uint64_t length, bool wrap = true);

private:
    std::uint64_t size_bytes_ = 0;
    std::map<std::uint64_t, std::uint8_t> bytes_;

    std::uint64_t normalize(std::uint64_t address, bool wrap) const;
    void erase_linear(std::uint64_t address, std::uint64_t length);
};

} // namespace flash_model

#endif
