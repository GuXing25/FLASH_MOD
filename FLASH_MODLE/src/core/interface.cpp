#include "flash_model/interface.hpp"

namespace flash_model {

InterfaceEngine::InterfaceEngine(const ModelConfig& config)
    : config_(config.interface)
{
}

bool InterfaceEngine::enabled() const
{
    return config_.enabled;
}

double InterfaceEngine::bytes_per_us() const
{
    if (!enabled() || config_.clock_mhz <= 0.0 || config_.data_width_bits == 0 ||
        config_.lanes == 0) {
        return 0.0;
    }

    // MHz is cycles per microsecond. data_width_bits is the modeled transfer
    // width of one lane, so multiplying by lanes gives aggregate link width.
    return config_.clock_mhz *
           static_cast<double>(config_.lanes) *
           static_cast<double>(config_.data_width_bits) / 8.0;
}

std::uint64_t InterfaceEngine::packet_count(std::uint64_t payload_bytes) const
{
    if (payload_bytes == 0) return 0;
    if (config_.max_payload_bytes == 0) return 1;
    return (payload_bytes + config_.max_payload_bytes - 1) /
           config_.max_payload_bytes;
}

double InterfaceEngine::transfer_time_us(const InterfaceTransaction& transaction) const
{
    return transfer_time_us(transaction.request_bytes,
                            transaction.response_bytes,
                            transaction.turnaround);
}

double InterfaceEngine::transfer_time_us(std::uint64_t request_bytes,
                                         std::uint64_t response_bytes,
                                         bool turnaround) const
{
    const double bandwidth = bytes_per_us();
    if (bandwidth <= 0.0) return 0.0;

    const std::uint64_t payload_bytes = request_bytes + response_bytes;
    const std::uint64_t overhead_bytes =
        packet_count(payload_bytes) * config_.packet_overhead_bytes;
    const double transfer_us =
        static_cast<double>(payload_bytes + overhead_bytes) / bandwidth;

    return config_.fixed_latency_us +
           (turnaround ? config_.turnaround_us : 0.0) +
           transfer_us;
}

} // namespace flash_model
