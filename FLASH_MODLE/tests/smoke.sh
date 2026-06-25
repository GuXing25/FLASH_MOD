#!/usr/bin/env sh
set -eu

cd "$(dirname "$0")/.."

./flash_modle_sim configs/demo_nor.yaml --self-test > /tmp/flash_modle_nor.out
grep -q "Test Result: PASS" /tmp/flash_modle_nor.out

./flash_modle_sim configs/demo_nor_block_protect.yaml --self-test > /tmp/flash_modle_nor_bp.out
grep -q "Test Result: PASS" /tmp/flash_modle_nor_bp.out
grep -q "NOR block protect rejects erase in protected range" /tmp/flash_modle_nor_bp.out
grep -q "NOR block protect rejects program in protected range" /tmp/flash_modle_nor_bp.out

./flash_modle_sim configs/demo_spinand.yaml --self-test > /tmp/flash_modle_nand.out
grep -q "Test Result: PASS" /tmp/flash_modle_nand.out
grep -q "block protect rejects erase before unlock" /tmp/flash_modle_nand.out
grep -q "ECC reserved spare range rejects program" /tmp/flash_modle_nand.out
grep -q "strict sequential program rejects skipped page" /tmp/flash_modle_nand.out

if ./flash_modle_sim configs/invalid_nor_with_nand_command.yaml --validate-only > /tmp/flash_modle_invalid.out 2>&1; then
    echo "invalid config unexpectedly passed validation" >&2
    exit 1
fi
grep -q "NOR config enables SPI-NAND-only commands" /tmp/flash_modle_invalid.out

echo "FLASH_MODLE smoke tests PASS"
