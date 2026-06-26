# datasheet_tq Profile Mapping

This document maps the extracted datasheet information in `../datasheet_tq` into
FLASH_MODLE configuration profiles.

## Added Profiles

| Device | Type | Config | Policy | Status |
|---|---|---|---|---|
| BY25Q64AS | SPI NOR | `configs/nor_by25q64as.yaml` | `boya_family` | validate-only covered |
| GD25LE128E | SPI NOR | `configs/nor_gd25le128e.yaml` | `gigadevice_family` | validate-only covered |
| GD5F1GM7UEYIGR | SPI NAND | `configs/nand_gd5f1gm7ue.yaml` | `gigadevice_family` | validate-only covered |
| M25P40 | SPI NOR | `configs/nor_m25p40.yaml` | `micron_family` | validate-only covered |
| MX25L25645G | SPI NOR | `configs/nor_mx25l25645g.yaml` | `macronix_family` | validate-only covered |
| W25Q32JVSSIQ | SPI NOR | `configs/nor_w25q32jv.yaml` | `winbond_family` | validate-only covered |
| W25N01GVZEIG | SPI NAND | `configs/nand_w25n01gv.yaml` | `winbond_family` | validate-only covered |
| MT29F2G01ABAGDWB | SPI NAND | `configs/nand_mt29f2g01.yaml` | `micron_family` | validate-only covered |

## Fields Mapped Into Schema

- `device`: name, class, manufacturer, part number, ID, optional simulated Unique ID.
- `geometry`: NOR array/page/sector/32KiB-block/64KiB-block sizes; NAND main/spare/pages-per-block/blocks/planes.
- `timing`: read/page-read, program, erase, 32KiB erase, chip erase, reset, power-up/write-protect windows.
- `commands`: read ID, reset, SFDP, Unique ID, NAND parameter page, NOR read/program/sector erase/32KiB erase/block erase/chip erase, SPI-NAND read/cache/program/block erase.
- `registers`: SPI-NAND A0h/B0h/C0h/D0h defaults and dynamic status.
- `capabilities`: quad, block protect, suspend/resume, Unique ID, SFDP, NAND parameter page, OTP, security register, ECC status, bad block, die/plane select.
- `constraints`: WEL requirement, partial program count, strict sequential program, ECC-reserved spare range, address width, Unique ID size, SFDP size, parameter page size.
- `evidence`: each profile records `datasheet_tq` line references for key fields.

## Newly Implemented From Extraction

- NOR `nor_block_erase` command path using `geometry.block_size`.
- NOR `nor_block32_erase` command path using `geometry.block32_size`.
- NOR `nor_chip_erase` command path using `geometry.memory_size`.
- NOR `read_unique_id` command path with configured or deterministic simulated data.
- NOR `read_sfdp` command path with a bounded SFDP table and standard signature.
- Macronix MX25L25645G profile with 4-byte address mode hooks.
- SPI-NAND `read_unique_id` command path, shared with NOR through the `unique_id` capability.
- SPI-NAND `read_parameter_page` command path with an ONFI-style signature and profile-derived manufacturer/model fields.
- SPI-NAND generated parameter pages now encode page geometry, blocks per unit,
  bad-block maximum, partial-program count, ECC capability, and configured
  tPROG/tBERS/tR timing fields, with redundant 256-byte copies.
- W25N01GV profile now enables Unique ID, parameter page, OTP access commands,
  10 OTP pages, bad-block marker offset, minimum valid block count, and ECC bits.
- MT29F2G01 profile now enables Unique ID, parameter page, bad-block marker
  offset, minimum valid block count, and 8-bit ECC capability.
- GigaDevice GD5F1GM7UEYIGR profile with 2K+128 page geometry, ECC-reserved spare range, 10 OTP pages, UID, parameter page, and copy-back.
- Micron M25P40 profile with legacy 64KiB-sector erase and 16-byte UID.
- GigaDevice GD25LE128E profile with 128Mbit geometry, 32KiB/64KiB erase, 128-bit UID, SFDP, and 3x1024B security registers.
- NOR page program wrap inside the same programmable page.
- NOR Deep Power-Down / Release command state.
- NOR Security Register read / erase / program with 1->0 semantics.
- NOR Security Register lock command, backed by configurable register count and size.
- Program/Erase Suspend and Resume with a resumable busy window.
- SPI-NAND OTP mode with independent OTP-page storage.
- SPI-NAND die/plane select and read-retry runtime state hooks.
- Builder support for both file-based configs and in-memory `ModelConfig` objects.
- Smoke validation for all eight extracted profiles.

## Still Not Fully Modeled

- NOR BP/TB/CMP/SEC/WPS protection-table decoding.
- NOR status register protection and `/WP` pin effects.
- Vendor-specific Security Register lock-bit encoding.
- Vendor-specific Program/Erase Suspend timing restrictions.
- Full SFDP parameter tables beyond the basic modeled SFDP region.
- Full SPI-NAND parameter page fields beyond the basic modeled ONFI-style region.
- Macronix secured OTP area and security-register status bits.
- GigaDevice GD5F1 factory bad block scan and minimum valid block count enforcement.
- SPI-NAND bad block LUT / BBM link table.
- SPI-NAND factory bad block scan from spare markers.
- SPI-NAND OTP lock behavior and vendor-specific OTP address maps.
- SPI-NAND continuous/cache pipeline read behavior.
- Micron ECC correction strength and refresh recommendation status.
- Die/plane select address remapping beyond runtime state selection.
