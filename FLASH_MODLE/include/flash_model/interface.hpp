#ifndef FLASH_MODEL_INTERFACE_HPP
#define FLASH_MODEL_INTERFACE_HPP

#include "flash_model/config.hpp"

#include <cstdint>

namespace flash_model {

struct InterfaceTransaction {
    std::uint64_t request_bytes = 0;
    std::uint64_t response_bytes = 0;
    bool turnaround = true;
};

// Initial behavior-level model for the chiplet-to-chiplet path between the
// host/controller side and the flash model. The engine converts a logical
// transaction into elapsed time; it does not expose pin-level or electrical
// details, so it can be refined later without changing storage or command code.
class InterfaceEngine {
public:
    explicit InterfaceEngine(const ModelConfig& config);

    bool enabled() const;
    double transfer_time_us(const InterfaceTransaction& transaction) const;
    double transfer_time_us(std::uint64_t request_bytes,
                            std::uint64_t response_bytes,
                            bool turnaround = true) const;

private:
    InterfaceConfig config_;

    double bytes_per_us() const;
    std::uint64_t packet_count(std::uint64_t payload_bytes) const;
};

} // namespace flash_model

#endif
