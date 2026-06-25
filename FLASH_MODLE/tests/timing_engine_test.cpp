#include "flash_model/timing_engine.hpp"

#include <cassert>
#include <iostream>

using namespace flash_model;

int main()
{
    TimingEngine timing;
    assert(timing.now_us() == 0.0);
    assert(!timing.busy());

    timing.advance(0.5);
    assert(timing.now_us() == 0.5);

    timing.start_busy(10.0);
    assert(timing.busy());
    assert(timing.now_us() == 0.5);

    timing.advance(9.0);
    assert(timing.busy());
    assert(timing.now_us() == 9.5);

    timing.advance(1.0);
    assert(!timing.busy());
    assert(timing.now_us() == 10.5);

    timing.start_busy(4.0);
    timing.wait_ready();
    assert(!timing.busy());
    assert(timing.now_us() == 14.5);

    timing.start_busy(8.0);
    timing.clear_busy();
    assert(!timing.busy());
    assert(timing.now_us() == 14.5);

    timing.start_busy(0.0);
    assert(!timing.busy());

    std::cout << "TimingEngine tests PASS\n";
    return 0;
}
