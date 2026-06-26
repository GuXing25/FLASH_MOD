#ifndef FLASH_MODEL_TIMING_HPP
#define FLASH_MODEL_TIMING_HPP

namespace flash_model {

// TimingEngine 维护单个 outstanding busy 窗口和单调递增的模型时间。
class TimingEngine {
public:
    double now_us() const { return now_us_; }

    // busy 会先根据 now_us_ 自动完成到期操作。
    bool busy();
    // advance 推进模型时间，并触发 busy 到期检查。
    void advance(double delta_us);
    // start_busy 开启一个新的 busy 窗口；当前模型一次只跟踪一个后台操作。
    void start_busy(double duration_us);
    // suspend/resume 保存并恢复剩余 busy 时间。
    bool suspend();
    bool resume();
    bool suspended() const { return suspended_; }
    double suspended_remaining_us() const { return suspended_remaining_us_; }
    // wait_ready 直接把时间推进到 busy 结束点。
    void wait_ready();
    // clear_busy 用于 reset 等强制终止后台状态的命令。
    void clear_busy();

private:
    double now_us_ = 0.0; // 当前模型时间。
    bool busy_ = false; // 是否存在未完成后台操作。
    double busy_until_us_ = 0.0; // busy 预计结束时间。
    bool suspended_ = false; // 是否处于暂停态。
    double suspended_remaining_us_ = 0.0; // 暂停时保存的剩余时间。

    void complete_if_ready();
};

} // namespace flash_model

#endif
