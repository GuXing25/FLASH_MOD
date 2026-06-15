#include "config_parser.h"
#include "flash_core.h"
#include "flash_test.h"
#include "ftl.h"

#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

using namespace flashmod;

int main(int argc, char** argv)
{
    try {
        // NOR-only 项目默认跑 demo NOR，自测默认开启；传入任意配置路径即可替换 profile。
        std::string config = "configs/demo_nor.conf";
        bool self_test = true;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--no-test") self_test = false;
            else if (arg == "--self-test") self_test = true;
            else config = arg;
        }

        // 配置画像、器件行为核心、FTL 外观层依次组装，体现当前框架的主调用链。
        DeviceProfile profile = load_profile(config);
        print_profile(profile);

        FlashCore core(profile);
        FTL ftl(core);

        bool ok = true;
        if (self_test) ok = flash_test_run(ftl);

        std::cout << "Test Result: " << (ok ? "PASS" : "FAIL") << "\n";
        std::cout << "Final Time: " << std::fixed << std::setprecision(3)
                  << ftl.time_us() << " us\n";
        return ok ? 0 : 2;
    } catch (const std::exception& e) {
        std::cerr << "[fatal] " << e.what() << "\n";
        return 1;
    }
}
