#include "flash_model/timing_engine.hpp"

#include <algorithm>

namespace flash_model {

void TimingEngine::complete_if_ready()
{
    if (busy_ && now_us_ >= busy_until_us_) busy_ = false;
}

bool TimingEngine::busy()
{
    complete_if_ready();
    return busy_;
}

void TimingEngine::advance(double delta_us)
{
    if (delta_us > 0.0) now_us_ += delta_us;
    complete_if_ready();
}

void TimingEngine::start_busy(double duration_us)
{
    if (duration_us <= 0.0) {
        busy_ = false;
        busy_until_us_ = now_us_;
        return;
    }

    busy_ = true;
    busy_until_us_ = now_us_ + duration_us;
}

void TimingEngine::wait_ready()
{
    if (busy_) {
        now_us_ = std::max(now_us_, busy_until_us_);
        busy_ = false;
    }
}

void TimingEngine::clear_busy()
{
    busy_ = false;
    busy_until_us_ = now_us_;
}

} // namespace flash_model
