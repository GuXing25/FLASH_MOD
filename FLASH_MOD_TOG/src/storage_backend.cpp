#include "storage_backend.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace flashmod {

StorageBackend::~StorageBackend()
{
    close();
}

void StorageBackend::open(const std::string& path, std::uint64_t size, bool recreate_file)
{
    path_ = path;
    std::filesystem::path fs_path(path_);
    // 自动创建父目录，方便配置文件把 storage_file 指到独立输出目录。
    if (fs_path.has_parent_path()) std::filesystem::create_directories(fs_path.parent_path());
    if (recreate_file || !std::filesystem::exists(fs_path)) recreate(size);

    file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_) throw std::runtime_error("cannot open storage file: " + path_);
}

void StorageBackend::close()
{
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

bool StorageBackend::is_open() const
{
    return file_.is_open();
}

void StorageBackend::recreate(std::uint64_t size)
{
    std::ofstream out(path_, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot create storage file: " + path_);

    // Flash 擦除态为 0xFF，因此新建镜像时全片填 0xFF。
    const std::size_t chunk_size = 1024 * 1024;
    std::vector<char> chunk(chunk_size, static_cast<char>(0xFF));
    while (size > 0) {
        const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(size, chunk.size()));
        out.write(chunk.data(), static_cast<std::streamsize>(n));
        size -= n;
    }
}

std::vector<std::uint8_t> StorageBackend::read(std::uint64_t address,
                                               std::size_t length,
                                               bool wrap,
                                               std::uint64_t total_size)
{
    // 读越界或文件不可用时默认返回 0xFF，等价于读到擦除态区域。
    std::vector<std::uint8_t> out(length, 0xFF);
    if (!file_.is_open() || total_size == 0) return out;

    for (std::size_t i = 0; i < length; ++i) {
        // wrap=false 时，超过 total_size 的部分保持默认 0xFF 并提前停止。
        const std::uint64_t at = wrap ? (address + i) % total_size : address + i;
        if (at >= total_size) break;
        file_.clear();
        file_.seekg(static_cast<std::streamoff>(at));
        char ch = static_cast<char>(0xFF);
        file_.read(&ch, 1);
        if (file_) out[i] = static_cast<std::uint8_t>(ch);
    }
    file_.clear();
    return out;
}

void StorageBackend::write(std::uint64_t address,
                           const std::vector<std::uint8_t>& data,
                           bool wrap,
                           std::uint64_t total_size)
{
    // 这里是原始写入，不自动执行 old&new；FlashCore 会在 program 完成时先算好结果。
    if (!file_.is_open() || total_size == 0) return;

    for (std::size_t i = 0; i < data.size(); ++i) {
        const std::uint64_t at = wrap ? (address + i) % total_size : address + i;
        if (at >= total_size) break;
        file_.clear();
        file_.seekp(static_cast<std::streamoff>(at));
        const char ch = static_cast<char>(data[i]);
        file_.write(&ch, 1);
    }
    file_.flush();
    file_.clear();
}

void StorageBackend::erase(std::uint64_t address,
                           std::uint64_t length,
                           bool wrap,
                           std::uint64_t total_size)
{
    // erase 只是把指定范围写回 0xFF，使用小块循环避免一次分配很大的芯片容量。
    const std::size_t chunk_size = 4096;
    std::vector<std::uint8_t> chunk(chunk_size, 0xFF);
    std::uint64_t remaining = length;
    std::uint64_t cursor = address;
    while (remaining > 0) {
        const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, chunk.size()));
        write(cursor, std::vector<std::uint8_t>(chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(n)),
              wrap, total_size);
        cursor += n;
        remaining -= n;
    }
}

} // namespace flashmod
