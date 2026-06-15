#include "flash_test.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace flashmod {
namespace {

void print_hex(const std::string& label, const std::vector<std::uint8_t>& data, std::size_t limit = 16)
{
    // 自测输出只打印前若干字节，避免大页 NAND 把日志刷得过长。
    std::cout << label;
    const std::size_t n = std::min(limit, data.size());
    for (std::size_t i = 0; i < n; ++i) {
        std::cout << " " << std::hex << std::uppercase << std::setw(2)
                  << std::setfill('0') << static_cast<int>(data[i]);
    }
    std::cout << std::dec << std::nouppercase << std::setfill(' ');
    if (data.size() > n) std::cout << " ...";
    std::cout << "\n";
}

bool all_equal(const std::vector<std::uint8_t>& data, std::uint8_t value)
{
    // 用于验证擦除/编程后的读回数据是否符合预期。
    return std::all_of(data.begin(), data.end(),
                       [value](std::uint8_t b) { return b == value; });
}

void require_ok(const OperationResult& result)
{
    // 统一打印命令结果；失败直接抛异常，让外层 self-test 返回 FAIL。
    std::cout << "[cmd] " << result.message
              << " ok=" << (result.ok ? 1 : 0)
              << " now=" << std::fixed << std::setprecision(3)
              << result.now_us << "us\n";
    if (!result.ok) throw std::runtime_error(result.message);
}

bool run_spinand_self_test(FTL& ftl)
{
    bool ok = true;

    // SPI-NAND 基本流程：读 ID -> 解保护 -> block erase -> program load/execute -> page read/cache read。
    OperationResult id = ftl.read_id();
    require_ok(id);
    print_hex("[data] ID", id.data);

    if (ftl.profile().timings.t_puw_us > 0.0) {
        require_ok(ftl.wait_us(ftl.profile().timings.t_puw_us));
    }

    OperationResult a0 = ftl.get_feature(0xA0);
    require_ok(a0);
    if (!a0.data.empty() && (a0.data[0] & 0x7Cu) != 0) {
        // 一些 NAND 上电默认全保护；自测需要先清 block lock。
        require_ok(ftl.set_feature(0xA0, 0x00));
    }

    require_ok(ftl.write_enable());
    require_ok(ftl.block_erase(0));
    require_ok(ftl.wait_ready());

    std::vector<std::uint8_t> data(128, 0xA5);
    require_ok(ftl.write_enable());
    require_ok(ftl.program_load(0, data));
    require_ok(ftl.program_execute(0));
    require_ok(ftl.wait_ready());

    require_ok(ftl.page_read(0));
    require_ok(ftl.wait_ready());
    OperationResult read1 = ftl.read_from_cache(0, data.size());
    require_ok(read1);
    print_hex("[data] NAND cache read", read1.data);
    ok = ok && all_equal(read1.data, 0xA5);

    std::vector<std::uint8_t> patch(16, 0xF0);
    // random load 保留 cache 其余位置，只覆盖 column=0 的 16 字节。
    require_ok(ftl.write_enable());
    require_ok(ftl.program_load(0, patch, true));
    require_ok(ftl.program_execute(0));
    require_ok(ftl.wait_ready());

    require_ok(ftl.page_read(0));
    require_ok(ftl.wait_ready());
    OperationResult read2 = ftl.read_from_cache(0, patch.size());
    require_ok(read2);
    print_hex("[data] NAND partial old&new", read2.data);
    ok = ok && all_equal(read2.data, 0xA0);

    OperationResult c0 = ftl.get_feature(0xC0);
    require_ok(c0);
    print_hex("[status] C0", c0.data, 1);
    return ok;
}

} // namespace

bool flash_test_run(FTL& ftl)
{
    if (!ftl.profile().is_nand_like()) {
        throw std::runtime_error("NAND-only self-test received NOR profile");
    }
    return run_spinand_self_test(ftl);
}

} // namespace flashmod
