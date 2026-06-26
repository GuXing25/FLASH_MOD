# Command Transaction Layer

This layer keeps the data-exchange shape of each command separate from the
command behavior itself.

## Why It Exists

Before this layer, `FlashModel` directly called the interface engine with
hand-written byte counts. That made the model harder to extend because command
semantics, data exchange, and chiplet timing were mixed in one place.

The new flow is:

```text
FlashModel command
  -> command name + dynamic payload/response size
  -> TransactionConfig
  -> CommandTransfer
  -> InterfaceEngine
  -> TimingEngine
```

## Default Table

`src/core/command.cpp` contains the default transaction table for common NOR and
SPI-NAND facade commands. Examples:

- `nor_read`: opcode + current address width, read response payload.
- `nor_program`: opcode + current address width + write payload.
- `program_load`: opcode + 2 address bytes + write payload.
- `read_from_cache`: opcode + 2 address bytes + dummy byte + read response.
- `get_feature`: opcode + feature address + 1-byte response.

## Optional Config Override

Most profiles should rely on the default table. A config only needs an override
when the chiplet-facing transaction differs from the generic shape:

```yaml
transactions:
  nor_read:
    opcode_value: 0x03
    opcode_bytes: 1
    address_bytes: 0
    dummy_bytes: 0
    dummy_cycles: 0
    fixed_request_bytes: 0
    fixed_response_bytes: 0
    command_lanes: 1
    address_lanes: 1
    data_lanes: 1
    use_current_address_bytes: true
    write_payload: false
    read_response: true
    turnaround: true

  fast_read:
    alias_of: nor_read
    opcode_value: 0x0B
```

The override describes transaction metadata and data exchange. It does not
implement the storage behavior of the command; that remains in `FlashModel`
plus capabilities and policy.

## Metadata Scope

The layer now records:

- `opcode_value` and `opcode_bytes` for datasheet traceability.
- `dummy_bytes` for byte-based interface timing.
- `dummy_cycles` for preserving datasheet wording when cycles and bytes differ.
- `command_lanes`, `address_lanes`, and `data_lanes` as phase metadata.
- `alias_of` for vendor command names that reuse a canonical transaction shape.

## Current Limitations

- There is no queueing or multi-transaction pipeline yet.
- Phase lane metadata is recorded but not yet used to compute per-phase timing.
- `dummy_cycles` is recorded for evidence, while byte timing still uses `dummy_bytes`.
- Alias support maps transaction shape only; command behavior still uses canonical `FlashModel` methods.

Those are deliberate gaps until the real chiplet interface declaration and more
profile evidence are available.
