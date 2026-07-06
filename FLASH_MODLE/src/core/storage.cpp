#include "flash_model/storage.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
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

std::uint64_t SparseStorageBackend::normalize_offset(std::uint64_t address,
                                                     std::uint64_t offset,
                                                     bool wrap) const
{
    if (size_bytes_ == 0) throw std::runtime_error("storage size is zero");

    // wrap 模式下先分别取模再相加，避免 address + offset 在 uint64_t 上回绕。
    if (wrap) {
        return ((address % size_bytes_) + (offset % size_bytes_)) % size_bytes_;
    }

    // 非 wrap 模式下，溢出和越界都应作为地址错误暴露给调用方。
    if (offset > std::numeric_limits<std::uint64_t>::max() - address) {
        throw std::out_of_range("storage address out of range");
    }
    return normalize(address + offset, false);
}

std::vector<std::uint8_t> SparseStorageBackend::read(std::uint64_t address,
                                                     std::size_t length,
                                                     bool wrap) const
{
    std::vector<std::uint8_t> out(length, 0xFF);
    for (std::size_t i = 0; i < length; ++i) {
        const std::uint64_t offset = normalize_offset(address, i, wrap);
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
        const std::uint64_t offset = normalize_offset(address, i, wrap);
        const auto old_it = bytes_.find(offset);
        const std::uint8_t old_value = old_it == bytes_.end() ? 0xFF : old_it->second;
        const std::uint8_t next = static_cast<std::uint8_t>(old_value & data[i]);
        if (next == 0xFF) bytes_.erase(offset);
        else bytes_[offset] = next;
    }
}

void SparseStorageBackend::erase_linear(std::uint64_t address, std::uint64_t length)
{
    const std::uint64_t room = size_bytes_ - address;
    const std::uint64_t end = address + std::min(length, room);
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

void SparseStorageBackend::write_image(const std::string& path, std::size_t chunk_size) const
{
    if (chunk_size == 0) chunk_size = 1;

    const std::filesystem::path output_path(path);
    const std::filesystem::path parent = output_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to open storage image: " + path);
    }

    std::vector<std::uint8_t> chunk(chunk_size, 0xFF);
    for (std::uint64_t base = 0; base < size_bytes_;) {
        const std::uint64_t remaining = size_bytes_ - base;
        const std::size_t count =
            static_cast<std::size_t>(std::min<std::uint64_t>(remaining, chunk.size()));

        // 每个 chunk 先恢复为擦除态，再覆盖稀疏 map 中真正被编程过的字节。
        std::fill(chunk.begin(), chunk.begin() + count, 0xFF);
        auto it = bytes_.lower_bound(base);
        while (it != bytes_.end() && it->first < base + count) {
            chunk[static_cast<std::size_t>(it->first - base)] = it->second;
            ++it;
        }

        output.write(reinterpret_cast<const char*>(chunk.data()),
                     static_cast<std::streamsize>(count));
        if (!output) {
            throw std::runtime_error("failed to write storage image: " + path);
        }
        base += count;
    }
}

} // namespace flash_model
