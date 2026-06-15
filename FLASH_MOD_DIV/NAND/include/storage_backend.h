#ifndef FLASH_MOD_STORAGE_BACKEND_H
#define FLASH_MOD_STORAGE_BACKEND_H

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace flashmod {

// StorageBackend 把整片 Flash 阵列映射成一个二进制文件。它只提供 byte
// 级 read/write/erase 原语，不理解 page、block、WEL、old&new 等 Flash 语义。
class StorageBackend {
public:
    StorageBackend() = default;
    ~StorageBackend();

    // recreate=true 时每次启动都重建为全 0xFF；否则复用已有镜像文件，便于跨仿真保留内容。
    void open(const std::string& path, std::uint64_t size, bool recreate);
    void close();
    bool is_open() const;
    const std::string& path() const { return path_; }

    // wrap=true 时越界访问按 total_size 环回；wrap=false 时越界部分被截断。
    std::vector<std::uint8_t> read(std::uint64_t address,
                                   std::size_t length,
                                   bool wrap,
                                   std::uint64_t total_size);
    void write(std::uint64_t address,
               const std::vector<std::uint8_t>& data,
               bool wrap,
               std::uint64_t total_size);
    void erase(std::uint64_t address,
               std::uint64_t length,
               bool wrap,
               std::uint64_t total_size);

private:
    std::fstream file_;
    std::string path_;

    // 以擦除态 0xFF 创建一个指定大小的镜像文件。
    void recreate(std::uint64_t size);
};

} // namespace flashmod

#endif
