# Chiplet Interface Model

This document records the provisional chiplet-to-chiplet interface layer used by
the behavior-level Flash model. It is not a pin-level SPI/NAND PHY model.

## Position In The Architecture

```text
External AI / Datasheet extraction
  -> Structured Config + evidence
  -> Config Loader / Validator
  -> Model Builder
  -> FlashModel
      -> Command Facade
      -> InterfaceEngine
      -> TimingEngine
      -> Storage/Register/Capability/Policy
```

The interface layer sits between command execution and the timing engine. It
adds elapsed time for data exchange between chiplets, while the existing timing
fields still describe device-side operation latency such as program, erase, page
read, reset, and busy windows.

Command byte counts now flow through the command transaction layer:

```text
command name + dynamic payload length
  -> TransactionConfig
  -> CommandTransfer
  -> InterfaceEngine
```

## Current Transaction Fields

```yaml
interface:
  enabled: true
  name: demo_chiplet_link
  protocol: generic_chiplet
  lanes: 1
  data_width_bits: 32
  clock_mhz: 500
  fixed_latency_us: 0.05
  turnaround_us: 0.02
  packet_overhead_bytes: 8
  max_payload_bytes: 256
```

The current formula is:

```text
payload_bytes = request_bytes + response_bytes
packet_count = ceil(payload_bytes / max_payload_bytes)
transfer_bytes = payload_bytes + packet_count * packet_overhead_bytes
bandwidth_bytes_per_us = clock_mhz * lanes * data_width_bits / 8
interface_time_us =
    fixed_latency_us
  + optional turnaround_us
  + transfer_bytes / bandwidth_bytes_per_us
```

If `interface.enabled` is false, the interface contributes zero time and all
older profiles keep their previous behavior.

## What Is Modeled

- Command/address/payload/response data exchange cost.
- Fixed adapter/protocol setup latency.
- Direction-turnaround latency for read-like transactions.
- Packet header/CRC/flow-control overhead through `packet_overhead_bytes`.
- Fragmentation overhead through `max_payload_bytes`.

## What Is Not Modeled Yet

- Electrical pins, voltage, signal integrity, clock recovery, or PHY training.
- Cycle-accurate protocol state machines.
- Multiple outstanding transactions, queue depth, arbitration, or QoS.
- Retry/NAK/CRC error behavior.
- Separate command, address, data, and response virtual channels.
- Coherency or memory ordering rules across multiple chiplets.

These items should wait until the real interface declaration is available.

## Integration Points

- Public configuration: `include/flash_model/config.hpp`
- Command transaction API: `include/flash_model/command.hpp`
- Interface engine API: `include/flash_model/interface.hpp`
- Command transaction defaults: `src/core/command.cpp`
- Implementation: `src/core/interface.cpp`
- Command facade accounting: `src/model/model.cpp`
- Command transaction test: `tests/command_test.cpp`
- Unit test: `tests/interface_test.cpp`
