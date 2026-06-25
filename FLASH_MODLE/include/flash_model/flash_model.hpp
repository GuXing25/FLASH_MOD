#ifndef FLASH_MODEL_FLASH_MODEL_HPP
#define FLASH_MODEL_FLASH_MODEL_HPP

#include "flash_model/address_mapper.hpp"
#include "flash_model/capability_module.hpp"
#include "flash_model/config.hpp"
#include "flash_model/device_policy.hpp"
#include "flash_model/register_engine.hpp"
#include "flash_model/storage_backend.hpp"
#include "flash_model/timing_engine.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace flash_model {

struct OperationResult {
    bool ok = true;
    std::string message;
    std::vector<std::uint8_t> data;
    double now_us = 0.0;
};

// FlashModel 是当前可执行模型的外观层。
//
// 它负责把各个 Common Core 部件串起来：
// - RegisterEngine: WEL、状态寄存器、Feature Register。
// - AddressMapper: NOR/NAND 地址和 offset。
// - TimingEngine: 当前时间、busy 窗口、wait-ready。
// - StorageBackend: 擦除态 0xFF 与 1->0 program。
// - CapabilityModule/DevicePolicy: 可选能力和厂商策略挂载点。
//
// 这里仍保留了 NOR/SPI-NAND 的最小命令流程。随着模块成熟，应继续把
// 更细的能力行为迁移到 capability module 和 policy 中，让 FlashModel 保持薄外观。
class FlashModel {
public:
    FlashModel(ModelConfig config,
               std::unique_ptr<DevicePolicy> policy,
               std::vector<std::unique_ptr<CapabilityModule>> modules);

    FlashModel(FlashModel&&) noexcept = default;
    FlashModel& operator=(FlashModel&&) noexcept = default;
    FlashModel(const FlashModel&) = delete;
    FlashModel& operator=(const FlashModel&) = delete;

    const ModelConfig& config() const { return config_; }
    const RuntimeState& state() const { return registers_.state(); }
    double time_us() const { return timing_.now_us(); }
    bool busy();

    OperationResult read_id();
    OperationResult write_enable();
    OperationResult write_disable();
    OperationResult wait_ready();
    OperationResult reset();

    OperationResult nor_read(std::uint64_t address, std::size_t length);
    OperationResult nor_program(std::uint64_t address, const std::vector<std::uint8_t>& data);
    OperationResult nor_sector_erase(std::uint64_t address);

    OperationResult get_feature(std::uint8_t address);
    OperationResult set_feature(std::uint8_t address, std::uint8_t value);
    OperationResult block_erase(std::uint32_t block);
    OperationResult program_load(std::uint16_t column,
                                 const std::vector<std::uint8_t>& data,
                                 bool random_load = false);
    OperationResult program_execute(std::uint32_t page);
    OperationResult page_read(std::uint32_t page);
    OperationResult read_from_cache(std::uint16_t column, std::size_t length);

private:
    ModelConfig config_;
    std::unique_ptr<DevicePolicy> policy_;
    std::vector<std::unique_ptr<CapabilityModule>> modules_;
    RegisterEngine registers_;
    AddressMapper address_mapper_;
    TimingEngine timing_;
    SparseStorageBackend storage_;

    std::vector<std::uint8_t> cache_;
    bool cache_valid_ = false;
    bool cache_ecc_violation_ = false;
    std::map<std::uint32_t, std::uint32_t> partial_program_count_;
    std::map<std::uint32_t, std::uint32_t> next_page_by_block_;

    OperationResult result(bool ok, const std::string& message,
                           const std::vector<std::uint8_t>& data = {});
    bool require_ready(const char* op);
    bool require_wel(const char* op);
    bool nand_block_locked(std::uint32_t block) const;
    bool touches_ecc_reserved(std::uint16_t column, std::size_t length) const;
};

} // namespace flash_model

#endif
