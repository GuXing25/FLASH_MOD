#ifndef FLASH_MODEL_TIMING_ENGINE_HPP
#define FLASH_MODEL_TIMING_ENGINE_HPP

namespace flash_model {

// TimingEngine 是 Common Core 中的“时间和 busy 状态”基座。
//
// 它只维护模型内部的单调时间和一个当前 pending busy 窗口：
// - advance() 用于普通命令总线时间或固定延时推进。
// - start_busy() 用于 program/erase/page-read 这类内部操作。
// - wait_ready() 模拟上层轮询/等待到内部操作完成。
// - busy() 会顺手完成已经到期的 pending 操作。
//
// 当前版本只支持单 outstanding busy 操作。后续如果要做多 die/plane 并行、
// per-die busy、AUTO_COMPLETE 或典型/最大时序切换，应该优先扩展这里。
class TimingEngine {
public:
    double now_us() const { return now_us_; }

    bool busy();
    void advance(double delta_us);
    void start_busy(double duration_us);
    void wait_ready();
    void clear_busy();

private:
    double now_us_ = 0.0;
    bool busy_ = false;
    double busy_until_us_ = 0.0;

    void complete_if_ready();
};

} // namespace flash_model

#endif
