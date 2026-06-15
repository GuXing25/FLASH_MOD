#include "flash_core.h"

#include <algorithm>
#include <iostream>

namespace flashmod {
namespace {

// NOR 和 SPI-NAND 的状态位位置并不完全相同。FlashCore 内部用 wel_/wip_
// 做统一状态，再在 set_wel()/set_wip() 中同步回对应寄存器位。
const std::uint8_t SR_WIP = 0x01;
const std::uint8_t SR_WEL = 0x02;
const std::uint8_t NAND_BUSY = 0x01;
const std::uint8_t NAND_WEL = 0x02;
const std::uint8_t NAND_E_FAIL = 0x04;
const std::uint8_t NAND_P_FAIL = 0x08;
const std::uint8_t NAND_ECC_MASK = 0x30;

bool is_quad_mode(IoMode mode)
{
    return mode == IoMode::X4 || mode == IoMode::QuadIO || mode == IoMode::QuadDTR;
}

} // namespace

FlashCore::FlashCore(const DeviceProfile& profile) : profile_(profile), hardware_(profile_)
{
    // 构造顺序：先让硬件层准备好元数据和 storage 文件，再初始化寄存器/cache。
    hardware_.initialize();
    initialize_registers();
}

FlashCore::~FlashCore()
{
    // StorageBackend closes itself through FlashHardware.
}

void FlashCore::initialize_registers()
{
    // 复位/上电时寄存器回到配置文件给出的默认值；运行态状态位由后面的
    // set_wel()/set_wip() 再统一修正。
    sr1_ = profile_.status1_default;
    sr2_ = profile_.status2_default;
    sr3_ = profile_.status3_default;
    wel_ = false;
    wip_ = false;
    deep_power_down_ = false;
    program_ecc_area_violation_ = false;
    cache_.assign(profile_.effective_page_size(), 0xFF);
    cache_valid_ = false;

    features_.clear();
    // SPI-NAND 常见 feature register 地址：
    // 0xA0 block lock, 0xB0 configuration, 0xC0 status, 0xD0 die select,
    // 0xF0 vendor-specific/OTP 类寄存器。NOR 配置中这些字段通常不会被访问。
    features_[0xA0] = profile_.feature_a0_default;
    features_[0xB0] = profile_.feature_b0_default;
    features_[0xC0] = sr3_;
    features_[0xD0] = profile_.feature_d0_default;
    features_[0xF0] = profile_.feature_f0_default;

    set_wel(false);
    set_wip(false);
    power_on_until_us_ = profile_.timings.t_puw_us;
}

OperationResult FlashCore::make_result(bool ok, const std::string& message,
                                        double bus, double internal,
                                        const std::vector<std::uint8_t>& data)
{
    // 所有命令都返回统一结构，便于 FTL、自测或后续 trace 收集器处理。
    OperationResult r;
    r.ok = ok;
    r.message = message;
    r.bus_time_us = bus;
    r.internal_time_us = internal;
    r.now_us = now_us_;
    r.data = data;
    return r;
}

void FlashCore::advance_time(double delta_us)
{
    if (delta_us <= 0.0) return;
    now_us_ += delta_us;
    // 时间推进后，可能有内部操作刚好到期；这里自动完成，避免调用方忘记轮询。
    complete_pending_if_ready();
}

void FlashCore::start_pending(PendingOp op, double internal_us)
{
    // 接受 program/erase/read-page 命令后先进入 busy；真正修改阵列的动作放到
    // complete_pending_operation()，这样 AUTO_COMPLETE=0 时可以模拟异步 busy。
    pending_ = std::move(op);
    pending_.complete_time_us = now_us_ + internal_us;
    set_wip(true);

    if (profile_.auto_complete || internal_us <= 0.0) {
        now_us_ = pending_.complete_time_us;
        complete_pending_operation();
    }
}

void FlashCore::complete_pending_if_ready()
{
    if (pending_.kind == PendingKind::None) return;
    if (now_us_ + 1e-9 < pending_.complete_time_us) return;
    complete_pending_operation();
}

void FlashCore::complete_pending_operation()
{
    if (pending_.kind == PendingKind::None) return;

    switch (pending_.kind) {
    case PendingKind::NorProgram: {
        // NOR page program 支持从页内任意 offset 开始，并可配置是否页内 wrap。
        // pending_.valid 标记哪些 byte 属于本次命令；未覆盖位置保持原值。
        std::vector<std::uint8_t> page = hardware_.read_bytes(pending_.address, profile_.page_size);
        if (page.size() != profile_.page_size) page.assign(profile_.page_size, 0xFF);
        for (std::size_t i = 0; i < page.size() && i < pending_.valid.size(); ++i) {
            // Flash program 只能把 1 写成 0，不能把 0 写回 1，所以最终值是 old & new。
            if (pending_.valid[i]) page[i] = static_cast<std::uint8_t>(page[i] & pending_.data[i]);
        }
        hardware_.write_bytes(pending_.address, page);
        break;
    }
    case PendingKind::NorErase:
        // 擦除由硬件层统一写回 0xFF。
        hardware_.erase_range(pending_.address, pending_.size);
        break;
    case PendingKind::NandPageRead:
        // SPI-NAND 的 page read 先把阵列页搬到 cache register，之后 read_from_cache 才取数据。
        cache_ = hardware_.read_page(pending_.page);
        cache_valid_ = true;
        cached_page_ = pending_.page;
        sr3_ &= static_cast<std::uint8_t>(~NAND_ECC_MASK);
        break;
    case PendingKind::NandProgram: {
        // SPI-NAND program execute 把 cache register 写入目标页，同样遵守 old & new。
        std::vector<std::uint8_t> old = hardware_.read_page(pending_.page);
        if (old.size() != pending_.data.size()) old.assign(pending_.data.size(), 0xFF);
        for (std::size_t i = 0; i < old.size(); ++i) {
            old[i] = static_cast<std::uint8_t>(old[i] & pending_.data[i]);
        }
        hardware_.write_page(pending_.page, old);
        // 记录 partial program 次数和顺序编程进度，供下一次 PROGRAM_EXECUTE 检查。
        hardware_.note_page_program(pending_.block, hardware_.mapper().page_in_block(pending_.page));
        break;
    }
    case PendingKind::NandErase:
        // 擦除 block 后，对应页的 program_count/valid/next_program_page 也要重置。
        hardware_.erase_block(pending_.block);
        hardware_.reset_block_metadata(pending_.block);
        break;
    case PendingKind::Reset:
        break;
    case PendingKind::None:
        break;
    }

    pending_ = PendingOp();
    // 内部操作结束后清 busy；WEL 在命令接受时已经清除。
    set_wip(false);
}

bool FlashCore::is_busy()
{
    complete_pending_if_ready();
    return wip_;
}

bool FlashCore::require_ready(const char* op)
{
    complete_pending_if_ready();
    // 目前还没有完整 deep-power-down 命令入口，但保留状态检查，便于后续扩展。
    if (deep_power_down_) {
        std::cerr << "[FLASH_MOD] " << op << " rejected: deep power-down\n";
        return false;
    }
    if (wip_) {
        std::cerr << "[FLASH_MOD] " << op << " rejected: device busy\n";
        return false;
    }
    return true;
}

bool FlashCore::require_write_enable(const char* op)
{
    if (!profile_.require_wel) return true;
    if (!wel_) {
        std::cerr << "[FLASH_MOD] " << op << " ignored: WEL=0\n";
        return false;
    }
    return true;
}

bool FlashCore::require_quad_enable(const char* op, IoMode mode)
{
    if (!profile_.require_qe_for_quad || !is_quad_mode(mode)) return true;
    // 不同厂商 QE 位可能在 SR1/SR2 或 SPI-NAND feature 0xB0；这里用配置的
    // quad_enable_default 初始化，运行时按器件类别检查常见位置。
    const bool qe = profile_.is_nand_like()
        ? ((features_[0xB0] & 0x01u) != 0)
        : ((sr2_ & 0x02u) != 0 || (sr1_ & 0x40u) != 0);
    if (!qe) {
        std::cerr << "[FLASH_MOD] " << op << " rejected: QE=0\n";
        return false;
    }
    return true;
}

void FlashCore::set_wel(bool value)
{
    wel_ = value;
    // WEL 对 NOR 体现在 SR1 bit1；对 SPI-NAND 体现在 status feature 0xC0 bit1。
    if (profile_.is_nand_like()) {
        if (value) sr3_ |= NAND_WEL;
        else sr3_ &= static_cast<std::uint8_t>(~NAND_WEL);
        features_[0xC0] = sr3_;
    } else {
        if (value) sr1_ |= SR_WEL;
        else sr1_ &= static_cast<std::uint8_t>(~SR_WEL);
    }
}

void FlashCore::set_wip(bool value)
{
    wip_ = value;
    // NOR 常叫 WIP，SPI-NAND 常叫 BUSY/OIP；内部统一成 wip_。
    if (profile_.is_nand_like()) {
        if (value) sr3_ |= NAND_BUSY;
        else sr3_ &= static_cast<std::uint8_t>(~NAND_BUSY);
        features_[0xC0] = sr3_;
    } else {
        if (value) sr1_ |= SR_WIP;
        else sr1_ &= static_cast<std::uint8_t>(~SR_WIP);
    }
}

void FlashCore::set_program_fail(bool value)
{
    if (value) sr3_ |= NAND_P_FAIL;
    else sr3_ &= static_cast<std::uint8_t>(~NAND_P_FAIL);
    features_[0xC0] = sr3_;
}

void FlashCore::set_erase_fail(bool value)
{
    if (value) sr3_ |= NAND_E_FAIL;
    else sr3_ &= static_cast<std::uint8_t>(~NAND_E_FAIL);
    features_[0xC0] = sr3_;
}

void FlashCore::clear_fail_bits()
{
    set_program_fail(false);
    set_erase_fail(false);
}

OperationResult FlashCore::write_enable()
{
    complete_pending_if_ready();
    const double bus = bus_time_for_bytes(8, 0, IoMode::X1);
    advance_time(bus);
    // 一些芯片要求上电 tPUW 窗口内忽略写使能；配置为 0 时等价于不限制。
    if (now_us_ < power_on_until_us_) {
        return make_result(false, "WRITE_ENABLE ignored inside tPUW", bus);
    }
    set_wel(true);
    return make_result(true, "WRITE_ENABLE: WEL=1", bus);
}

OperationResult FlashCore::write_disable()
{
    const double bus = bus_time_for_bytes(8, 0, IoMode::X1);
    advance_time(bus);
    set_wel(false);
    return make_result(true, "WRITE_DISABLE: WEL=0", bus);
}

OperationResult FlashCore::reset()
{
    const double bus = bus_time_for_bytes(8, 0, IoMode::X1);
    advance_time(bus);
    // reset 立即恢复寄存器和 cache，再根据 t_reset_us 保持 busy 一段时间。
    initialize_registers();
    PendingOp op;
    op.kind = PendingKind::Reset;
    start_pending(op, profile_.timings.t_reset_us);
    return make_result(true, "RESET accepted", bus, profile_.timings.t_reset_us);
}

OperationResult FlashCore::wait_us(double delta_us)
{
    advance_time(delta_us);
    return make_result(true, "WAIT done");
}

OperationResult FlashCore::wait_ready()
{
    if (pending_.kind != PendingKind::None && wip_) {
        // wait_ready 是“阻塞直到内部操作完成”的便捷接口，直接把模型时间推进到完成点。
        now_us_ = std::max(now_us_, pending_.complete_time_us);
        complete_pending_operation();
    }
    return make_result(true, "WAIT_READY done");
}

std::uint8_t FlashCore::status(std::uint8_t index)
{
    complete_pending_if_ready();
    if (index == 1) return sr1_;
    if (index == 2) return sr2_;
    return sr3_;
}

OperationResult FlashCore::read_id()
{
    const double bus = bus_time_for_bytes(8, profile_.id_bytes.size(), IoMode::X1);
    advance_time(bus);
    return make_result(true, "READ_ID", bus, 0.0, profile_.id_bytes);
}

OperationResult FlashCore::read(std::uint64_t address, std::size_t length, IoMode mode)
{
    // NOR 读是直接阵列路径；NAND 需要 page_read -> read_from_cache。
    if (profile_.is_nand_like()) return make_result(false, "direct READ is only for NOR profiles");
    if (!require_ready("READ")) return make_result(false, "READ rejected");
    if (!require_quad_enable("READ", mode)) return make_result(false, "READ rejected: QE=0");
    if (!hardware_.mapper().valid_array_range(address, length)) return make_result(false, "READ rejected: address out of range");

    const double bus = bus_time_for_bytes(profile_.address_bytes * 8u + 8u, length, mode);
    std::vector<std::uint8_t> out = hardware_.read_bytes(hardware_.mapper().normalize_array_address(address), length);
    advance_time(bus);
    return make_result(true, "READ", bus, 0.0, out);
}

OperationResult FlashCore::program(std::uint64_t address, const std::vector<std::uint8_t>& data, IoMode mode)
{
    // NOR page program：命令接受时只准备一页 pending 数据，内部完成时才写 storage。
    if (profile_.is_nand_like()) return make_result(false, "direct PROGRAM is only for NOR profiles");
    if (!require_ready("PAGE_PROGRAM")) return make_result(false, "PAGE_PROGRAM rejected");
    if (!require_write_enable("PAGE_PROGRAM")) return make_result(false, "PAGE_PROGRAM ignored: WEL=0");
    if (!require_quad_enable("PAGE_PROGRAM", mode)) return make_result(false, "PAGE_PROGRAM rejected: QE=0");
    if (data.empty()) return make_result(false, "PAGE_PROGRAM rejected: empty payload");
    if (!hardware_.mapper().valid_array_range(address, 1)) return make_result(false, "PAGE_PROGRAM rejected: address out of range");

    const std::uint64_t normalized = hardware_.mapper().normalize_array_address(address);
    const std::uint64_t page_base = normalized - (normalized % profile_.page_size);
    // page/valid 分开存储，支持“只改页内一段”的 program，完成时不影响同页其他 byte。
    std::vector<std::uint8_t> page(profile_.page_size, 0xFF);
    std::vector<std::uint8_t> valid(profile_.page_size, 0);
    std::uint32_t offset = static_cast<std::uint32_t>(normalized % profile_.page_size);
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (!profile_.page_program_wrap && offset + i >= profile_.page_size) break;
        // 多数 SPI NOR 页编程越过页尾会在页内回绕；PAGE_PROGRAM_WRAP=0 时则截断。
        const std::uint32_t at = static_cast<std::uint32_t>((offset + i) % profile_.page_size);
        page[at] = data[i];
        valid[at] = 1;
    }

    const double bus = bus_time_for_bytes(profile_.address_bytes * 8u + 8u, data.size(), mode);
    advance_time(bus);
    set_wel(false);
    PendingOp op;
    op.kind = PendingKind::NorProgram;
    op.address = page_base;
    op.size = profile_.page_size;
    op.data = std::move(page);
    op.valid = std::move(valid);
    start_pending(std::move(op), program_time());
    return make_result(true, "PAGE_PROGRAM accepted", bus, program_time());
}

OperationResult FlashCore::erase(std::uint64_t address, EraseKind kind)
{
    // NOR 擦除按 erase kind 对齐到对应边界；chip erase 强制从 0 开始覆盖全片。
    if (profile_.is_nand_like()) return make_result(false, "direct ERASE is only for NOR profiles");
    if (!require_ready("ERASE")) return make_result(false, "ERASE rejected");
    if (!require_write_enable("ERASE")) return make_result(false, "ERASE ignored: WEL=0");

    std::uint64_t size = profile_.sector_size;
    if (kind == EraseKind::Block32K) size = profile_.block32_size;
    else if (kind == EraseKind::Block64K) size = profile_.block64_size;
    else if (kind == EraseKind::Chip) size = profile_.total_size_bytes();

    std::uint64_t addr = kind == EraseKind::Chip ? 0 : hardware_.mapper().normalize_array_address(address);
    if (kind != EraseKind::Chip) addr = hardware_.mapper().align_down(addr, size);
    if (!hardware_.mapper().valid_array_range(addr, size)) return make_result(false, "ERASE rejected: address out of range");

    const double bus = bus_time_for_bytes(kind == EraseKind::Chip ? 8u : profile_.address_bytes * 8u + 8u, 0, IoMode::X1);
    const double internal = erase_time_for_kind(kind);
    advance_time(bus);
    set_wel(false);
    PendingOp op;
    op.kind = PendingKind::NorErase;
    op.address = addr;
    op.size = size;
    start_pending(std::move(op), internal);
    return make_result(true, "ERASE accepted: " + to_string(kind), bus, internal);
}

OperationResult FlashCore::get_feature(std::uint8_t address)
{
    if (!profile_.is_nand_like()) return make_result(false, "GET_FEATURE is only for NAND profiles");
    complete_pending_if_ready();
    // 0xC0 是运行态 status feature，需要返回 sr3_ 的最新镜像。
    const std::uint8_t value = address == 0xC0 ? sr3_ : features_[address];
    return make_result(true, "GET_FEATURE", bus_time_for_bytes(16, 1, IoMode::X1), 0.0, {value});
}

OperationResult FlashCore::set_feature(std::uint8_t address, std::uint8_t value)
{
    if (!profile_.is_nand_like()) return make_result(false, "SET_FEATURE is only for NAND profiles");
    if (!require_ready("SET_FEATURE")) return make_result(false, "SET_FEATURE rejected");
    // set_feature 暂时不逐位建模厂商私有限制；上层可以用配置默认值决定初态。
    features_[address] = value;
    return make_result(true, "SET_FEATURE");
}

OperationResult FlashCore::page_read(std::uint32_t page_address)
{
    // PAGE_READ 只触发阵列页到 cache 的内部传输，真正返回数据要等 READ_FROM_CACHE。
    if (!profile_.is_nand_like()) return make_result(false, "PAGE_READ is only for NAND profiles");
    if (!require_ready("PAGE_READ")) return make_result(false, "PAGE_READ rejected");
    if (!hardware_.valid_page(page_address)) return make_result(false, "PAGE_READ rejected: page out of range");

    const bool ecc_on = (features_[0xB0] & 0x10u) != 0;
    const double internal = ecc_on ? profile_.timings.t_read_ecc_on_us : profile_.timings.t_read_us;
    const double bus = bus_time_for_bytes(32, 0, IoMode::X1);
    advance_time(bus);
    PendingOp op;
    op.kind = PendingKind::NandPageRead;
    op.page = page_address;
    start_pending(std::move(op), internal);
    return make_result(true, "PAGE_READ accepted", bus, internal);
}

OperationResult FlashCore::read_from_cache(std::uint16_t column, std::size_t length, IoMode mode)
{
    // READ_FROM_CACHE 是 cache register 读，不再访问阵列，也不触发 t_read。
    if (!profile_.is_nand_like()) return make_result(false, "READ_FROM_CACHE is only for NAND profiles");
    if (!require_ready("READ_FROM_CACHE")) return make_result(false, "READ_FROM_CACHE rejected");
    if (!require_quad_enable("READ_FROM_CACHE", mode)) return make_result(false, "READ_FROM_CACHE rejected: QE=0");
    if (!cache_valid_) return make_result(false, "READ_FROM_CACHE rejected: cache invalid");
    if (column >= cache_.size()) return make_result(false, "READ_FROM_CACHE rejected: column out of range");

    const std::size_t n = std::min<std::size_t>(length, cache_.size() - column);
    std::vector<std::uint8_t> out(cache_.begin() + column, cache_.begin() + column + n);
    const double bus = bus_time_for_bytes(32, n, mode);
    advance_time(bus);
    return make_result(true, "READ_FROM_CACHE", bus, 0.0, out);
}

OperationResult FlashCore::program_load(std::uint16_t column,
                                         const std::vector<std::uint8_t>& data,
                                         bool random_load,
                                         IoMode mode)
{
    // PROGRAM_LOAD 填充 cache register。普通 load 先把 cache 清成 0xFF；
    // random load 保留已有 cache，只覆盖指定 column 范围。
    if (!profile_.is_nand_like()) return make_result(false, "PROGRAM_LOAD is only for NAND profiles");
    if (!require_ready("PROGRAM_LOAD")) return make_result(false, "PROGRAM_LOAD rejected");
    if (profile_.program_load_requires_wel && !require_write_enable("PROGRAM_LOAD")) {
        return make_result(false, "PROGRAM_LOAD ignored: WEL=0");
    }
    if (!require_quad_enable("PROGRAM_LOAD", mode)) return make_result(false, "PROGRAM_LOAD rejected: QE=0");
    if (column >= cache_.size()) return make_result(false, "PROGRAM_LOAD rejected: column out of range");

    if (!random_load) std::fill(cache_.begin(), cache_.end(), 0xFF);
    const std::size_t n = std::min<std::size_t>(data.size(), cache_.size() - column);
    std::copy(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(n),
              cache_.begin() + static_cast<std::ptrdiff_t>(column));
    cache_valid_ = true;
    if (touches_ecc_reserved(column, n)) program_ecc_area_violation_ = true;
    const double bus = bus_time_for_bytes(24, n, mode);
    advance_time(bus);
    return make_result(true, random_load ? "RANDOM_PROGRAM_LOAD" : "PROGRAM_LOAD", bus);
}

OperationResult FlashCore::program_execute(std::uint32_t page_address)
{
    // PROGRAM_EXECUTE 把 cache register 提交到阵列页，并产生内部 program busy。
    if (!profile_.is_nand_like()) return make_result(false, "PROGRAM_EXECUTE is only for NAND profiles");
    if (!require_ready("PROGRAM_EXECUTE")) return make_result(false, "PROGRAM_EXECUTE rejected");
    clear_fail_bits();
    if (!require_write_enable("PROGRAM_EXECUTE")) return make_result(false, "PROGRAM_EXECUTE ignored: WEL=0");
    if (!cache_valid_) {
        set_program_fail(true);
        set_wel(false);
        return make_result(false, "PROGRAM_EXECUTE rejected: cache invalid");
    }
    if (!hardware_.valid_page(page_address)) {
        set_program_fail(true);
        set_wel(false);
        return make_result(false, "PROGRAM_EXECUTE rejected: page out of range");
    }

    const std::uint32_t block = hardware_.mapper().block_of_page(page_address);
    const std::uint32_t in_block = hardware_.mapper().page_in_block(page_address);
    if (is_block_protected(block)) {
        set_program_fail(true);
        set_wel(false);
        return make_result(false, "PROGRAM_EXECUTE rejected: block protected/bad");
    }
    if (!hardware_.page_program_allowed(block, in_block, profile_.max_partial_programs, profile_.strict_sequential_program)) {
        set_program_fail(true);
        set_wel(false);
        return make_result(false, "PROGRAM_EXECUTE rejected: sequential or partial-program limit");
    }
    if (program_ecc_area_violation_) {
        // 如果内部 ECC 开启且 PROGRAM_LOAD 碰到 reserved spare 区，在 execute 阶段置 P_FAIL。
        set_program_fail(true);
        set_wel(false);
        program_ecc_area_violation_ = false;
        return make_result(false, "PROGRAM_EXECUTE rejected: ECC reserved spare area touched");
    }

    const double bus = bus_time_for_bytes(32, 0, IoMode::X1);
    const double internal = program_time();
    advance_time(bus);
    set_wel(false);
    PendingOp op;
    op.kind = PendingKind::NandProgram;
    op.page = page_address;
    op.block = block;
    op.data = cache_;
    cache_valid_ = false;
    start_pending(std::move(op), internal);
    return make_result(true, "PROGRAM_EXECUTE accepted", bus, internal);
}

OperationResult FlashCore::block_erase(std::uint32_t block_address)
{
    // NAND 擦除以 block 为单位，参数是 block index，不是 byte address。
    if (!profile_.is_nand_like()) return make_result(false, "BLOCK_ERASE is only for NAND profiles");
    if (!require_ready("BLOCK_ERASE")) return make_result(false, "BLOCK_ERASE rejected");
    clear_fail_bits();
    if (!require_write_enable("BLOCK_ERASE")) return make_result(false, "BLOCK_ERASE ignored: WEL=0");
    if (!hardware_.valid_block(block_address)) {
        set_erase_fail(true);
        set_wel(false);
        return make_result(false, "BLOCK_ERASE rejected: block out of range");
    }
    if (is_block_protected(block_address)) {
        set_erase_fail(true);
        set_wel(false);
        return make_result(false, "BLOCK_ERASE rejected: block protected/bad");
    }

    const double bus = bus_time_for_bytes(32, 0, IoMode::X1);
    const double internal = profile_.use_max_time ? profile_.timings.t_block_erase_max_us
                                                  : profile_.timings.t_block_erase_us;
    advance_time(bus);
    set_wel(false);
    PendingOp op;
    op.kind = PendingKind::NandErase;
    op.block = block_address;
    start_pending(std::move(op), internal);
    return make_result(true, "BLOCK_ERASE accepted", bus, internal);
}

bool FlashCore::is_block_protected(std::uint32_t block) const
{
    // 这里合并坏块、永久锁、默认全保护等策略；后续如果增加 BBM，也应在这里统一判断。
    if (!hardware_.valid_block(block)) return true;
    const BlockMeta& meta = hardware_.block(block);
    if (meta.permanent_locked) return true;
    if (profile_.strict_bad_block && meta.factory_bad) return true;
    if (profile_.default_all_protected && (features_.at(0xA0) & 0x7Cu) != 0) return true;
    return false;
}

double FlashCore::bus_time_for_bytes(std::size_t command_bits, std::size_t data_bytes, IoMode mode) const
{
    // 简化总线耗时模型：命令/地址按 command_bits 计入，数据按 lane 数折算。
    // MHz = bit/us，因此 bit / MHz 直接得到 us。
    double freq = profile_.timings.f_cmd_mhz;
    double lanes = 1.0;
    if (mode == IoMode::X2 || mode == IoMode::DualIO) lanes = 2.0;
    if (mode == IoMode::X4 || mode == IoMode::QuadIO) lanes = 4.0;
    if (mode == IoMode::QuadDTR) lanes = 8.0;
    if (is_quad_mode(mode)) freq = profile_.timings.f_quad_mhz;
    else if (data_bytes != 0) freq = profile_.timings.f_read_mhz;
    if (freq <= 0.0) return 0.0;
    return (static_cast<double>(command_bits) + (static_cast<double>(data_bytes) * 8.0 / lanes)) / freq;
}

double FlashCore::program_time() const
{
    return profile_.use_max_time ? profile_.timings.t_prog_max_us : profile_.timings.t_prog_us;
}

double FlashCore::erase_time_for_kind(EraseKind kind) const
{
    if (kind == EraseKind::Sector4K) return profile_.timings.t_sector_erase_us;
    if (kind == EraseKind::Block32K) return profile_.timings.t_block32_erase_us;
    if (kind == EraseKind::Block64K) return profile_.timings.t_block64_erase_us;
    return profile_.timings.t_chip_erase_us;
}

bool FlashCore::touches_ecc_reserved(std::uint16_t column, std::size_t length) const
{
    // 只有器件支持 ECC、ECC 当前开启、且配置了 reserved 区间时才进行限制。
    if (!profile_.ecc_supported || profile_.ecc_reserved_end == 0) return false;
    const std::map<std::uint8_t, std::uint8_t>::const_iterator it = features_.find(0xB0);
    const std::uint8_t feature_b0 = it == features_.end() ? 0 : it->second;
    if ((feature_b0 & 0x10u) == 0) return false;
    const std::uint32_t start = column;
    const std::uint32_t end = column + static_cast<std::uint32_t>(length);
    return start < profile_.ecc_reserved_end && end > profile_.ecc_reserved_start;
}

} // namespace flashmod
