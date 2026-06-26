#include "flash_model/interface.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace flash_model;

namespace {

void test_disabled_interface_has_zero_cost()
{
    ModelConfig config;
    InterfaceEngine interface(config);

    assert(!interface.enabled());
    assert(interface.transfer_time_us(1024, 1024) == 0.0);
}

void test_generic_chiplet_cost()
{
    ModelConfig config;
    config.interface.enabled = true;
    config.interface.protocol = "generic_chiplet";
    config.interface.lanes = 1;
    config.interface.data_width_bits = 32;
    config.interface.clock_mhz = 500.0;
    config.interface.fixed_latency_us = 0.05;
    config.interface.turnaround_us = 0.02;
    config.interface.packet_overhead_bytes = 8;
    config.interface.max_payload_bytes = 256;

    InterfaceEngine interface(config);

    // 500 cycles/us * 32 bits / 8 = 2000 bytes/us.
    // Payload 16 bytes plus one 8-byte packet overhead gives 24 bytes.
    const double read_time = interface.transfer_time_us(8, 8);
    assert(std::fabs(read_time - 0.082) < 1e-12);

    // Write-only transactions do not need request-to-response turnaround.
    const double write_time = interface.transfer_time_us(4, 0, false);
    assert(std::fabs(write_time - 0.056) < 1e-12);
}

void test_fragmentation_overhead()
{
    ModelConfig config;
    config.interface.enabled = true;
    config.interface.protocol = "generic_chiplet";
    config.interface.lanes = 2;
    config.interface.data_width_bits = 32;
    config.interface.clock_mhz = 250.0;
    config.interface.packet_overhead_bytes = 4;
    config.interface.max_payload_bytes = 16;

    InterfaceEngine interface(config);

    // Aggregate bandwidth is still 2000 bytes/us. A 40-byte payload splits
    // into 3 packets, so 12 overhead bytes are charged.
    const double time = interface.transfer_time_us(20, 20, false);
    assert(std::fabs(time - 0.026) < 1e-12);
}

} // namespace

int main()
{
    test_disabled_interface_has_zero_cost();
    test_generic_chiplet_cost();
    test_fragmentation_overhead();

    std::cout << "Interface tests PASS\n";
    return 0;
}
