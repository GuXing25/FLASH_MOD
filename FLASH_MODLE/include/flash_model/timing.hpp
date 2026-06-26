#ifndef FLASH_MODEL_TIMING_HPP
#define FLASH_MODEL_TIMING_HPP

namespace flash_model {

// Single outstanding busy window plus monotonic model time.
class TimingEngine {
public:
    double now_us() const { return now_us_; }

    bool busy();
    void advance(double delta_us);
    void start_busy(double duration_us);
    bool suspend();
    bool resume();
    bool suspended() const { return suspended_; }
    double suspended_remaining_us() const { return suspended_remaining_us_; }
    void wait_ready();
    void clear_busy();

private:
    double now_us_ = 0.0;
    bool busy_ = false;
    double busy_until_us_ = 0.0;
    bool suspended_ = false;
    double suspended_remaining_us_ = 0.0;

    void complete_if_ready();
};

} // namespace flash_model

#endif
