#include "flash_model/storage_backend.hpp"

#include <algorithm>
#include <stdexcept>

namespace flash_model {

SparseStorageBackend::SparseStorageBackend(std::uint64_t size_bytes)
    : size_bytes_(size_bytes)
{
}

void SparseStorageBackend::resize(std::uint64_t size_bytes)
{
    size_bytes_ = size_bytes;
    bytes_.clear();
}

std::uint64_t SparseStorageBackend::normalize(std::uint64_t address, bool wrap) const
{
    if (size_bytes_ == 0) throw std::runtime_error("storage size is zero");
    if (address < size_bytes_) return address;
    if (wrap) return address % size_bytes_;
    throw std::out_of_range("storage address out of range");
}

std::vector<std::uint8_t> SparseStorageBackend::read(std::uint64_t address,
                                                     std::size_t length,
                                                     bool wrap) const
{
    std::vector<std::uint8_t> out(length, 0xFF);
    for (std::size_t i = 0; i < length; ++i) {
        const std::uint64_t offset = normalize(address + i, wrap);
        const auto it = bytes_.find(offset);
        if (it != bytes_.end()) out[i] = it->second;
    }
    return out;
}

void SparseStorageBackend::program_and(std::uint64_t address,
                                       const std::vector<std::uint8_t>& data,
                                       bool wrap)
{
    for (std::size_t i = 0; i < data.size(); ++i) {
        const std::uint64_t offset = normalize(address + i, wrap);
        const auto old_it = bytes_.find(offset);
        const std::uint8_t old_value = old_it == bytes_.end() ? 0xFF : old_it->second;
        const std::uint8_t next = static_cast<std::uint8_t>(old_value & data[i]);
        if (next == 0xFF) bytes_.erase(offset);
        else bytes_[offset] = next;
    }
}

void SparseStorageBackend::erase_linear(std::uint64_t address, std::uint64_t length)
{
    const std::uint64_t end = std::min<std::uint64_t>(size_bytes_, address + length);
    auto it = bytes_.lower_bound(address);
    while (it != bytes_.end() && it->first < end) {
        it = bytes_.erase(it);
    }
}

void SparseStorageBackend::erase(std::uint64_t address, std::uint64_t length, bool wrap)
{
    if (size_bytes_ == 0 || length == 0) return;
    if (length >= size_bytes_) {
        bytes_.clear();
        return;
    }

    const std::uint64_t start = normalize(address, wrap);
    if (!wrap || start + length <= size_bytes_) {
        erase_linear(start, length);
        return;
    }

    const std::uint64_t first = size_bytes_ - start;
    erase_linear(start, first);
    erase_linear(0, length - first);
}

} // namespace flash_model
