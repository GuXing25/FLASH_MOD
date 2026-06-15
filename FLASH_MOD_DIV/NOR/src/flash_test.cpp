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

bool run_nor_self_test(FTL& ftl)
{
    bool ok = true;
    const std::uint64_t addr = 0x120;

    // NOR 基本流程：读 ID -> 等待 tPUW -> 擦除 sector -> 页编程 -> 读回。
    OperationResult id = ftl.read_id();
    require_ok(id);
    print_hex("[data] ID", id.data);

    if (ftl.profile().timings.t_puw_us > 0.0) {
        require_ok(ftl.wait_us(ftl.profile().timings.t_puw_us));
    }

    require_ok(ftl.write_enable());
    require_ok(ftl.erase(addr, EraseKind::Sector4K));
    require_ok(ftl.wait_ready());

    std::vector<std::uint8_t> page(64, 0xAA);
    require_ok(ftl.write_enable());
    require_ok(ftl.program(addr, page));
    require_ok(ftl.wait_ready());

    OperationResult read1 = ftl.read(addr, page.size());
    require_ok(read1);
    print_hex("[data] NOR read", read1.data);
    ok = ok && all_equal(read1.data, 0xAA);

    std::vector<std::uint8_t> patch(16, 0xF0);
    // 第二次编程不擦除，用 0xAA & 0xF0 == 0xA0 验证 Flash 的 1->0 约束。
    require_ok(ftl.write_enable());
    require_ok(ftl.program(addr, patch));
    require_ok(ftl.wait_ready());

    OperationResult read2 = ftl.read(addr, patch.size());
    require_ok(read2);
    print_hex("[data] NOR old&new", read2.data);
    ok = ok && all_equal(read2.data, 0xA0);

    std::cout << "[status] SR1=0x" << std::hex << static_cast<int>(ftl.status(1))
              << " SR2=0x" << static_cast<int>(ftl.status(2))
              << " SR3=0x" << static_cast<int>(ftl.status(3))
              << std::dec << "\n";
    return ok;
}

} // namespace

bool flash_test_run(FTL& ftl)
{
    if (ftl.profile().is_nand_like()) {
        throw std::runtime_error("NOR-only self-test received NAND profile");
    }
    return run_nor_self_test(ftl);
}

} // namespace flashmod
