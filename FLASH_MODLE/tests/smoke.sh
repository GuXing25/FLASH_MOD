#!/usr/bin/env sh
set -eu

cd "$(dirname "$0")/.."

./flash_modle_sim configs/demo_nor.yaml --self-test > /tmp/flash_modle_nor.out
grep -q "Test Result: PASS" /tmp/flash_modle_nor.out
grep -q "Unique ID" /tmp/flash_modle_nor.out
grep -q "SFDP" /tmp/flash_modle_nor.out
grep -q "NOR page wrap head" /tmp/flash_modle_nor.out
grep -q "NOR_BLOCK32_ERASE accepted" /tmp/flash_modle_nor.out
grep -q "NOR_BLOCK_ERASE accepted" /tmp/flash_modle_nor.out
grep -q "NOR_CHIP_ERASE accepted" /tmp/flash_modle_nor.out
grep -q "deep power-down rejects normal read" /tmp/flash_modle_nor.out
grep -q "SUSPEND" /tmp/flash_modle_nor.out
grep -q "RESUME" /tmp/flash_modle_nor.out
grep -q "NOR security read" /tmp/flash_modle_nor.out
grep -q "NOR security old&new" /tmp/flash_modle_nor.out
grep -q "NOR security register lock rejects program" /tmp/flash_modle_nor.out

./flash_modle_sim configs/demo_nor_block_protect.yaml --self-test > /tmp/flash_modle_nor_bp.out
grep -q "Test Result: PASS" /tmp/flash_modle_nor_bp.out
grep -q "NOR block protect rejects erase in protected range" /tmp/flash_modle_nor_bp.out
grep -q "NOR block protect rejects program in protected range" /tmp/flash_modle_nor_bp.out

./flash_modle_sim configs/demo_spinand.yaml --self-test > /tmp/flash_modle_nand.out
grep -q "Test Result: PASS" /tmp/flash_modle_nand.out
grep -q "block protect rejects erase before unlock" /tmp/flash_modle_nand.out
grep -q "NAND OTP read" /tmp/flash_modle_nand.out
grep -q "NAND OTP mode rejects block erase" /tmp/flash_modle_nand.out
grep -q "ECC reserved spare range rejects program" /tmp/flash_modle_nand.out
grep -q "strict sequential program rejects skipped page" /tmp/flash_modle_nand.out
grep -q "COPY_BACK accepted" /tmp/flash_modle_nand.out
grep -q "bad block management rejects erase" /tmp/flash_modle_nand.out
grep -q "bad block management rejects program" /tmp/flash_modle_nand.out

for profile in \
    configs/by25q64as.yaml \
    configs/gd25le128e.yaml \
    configs/gd5f1gm7ue.yaml \
    configs/m25p40.yaml \
    configs/mx25l25645g.yaml \
    configs/w25q32jv.yaml \
    configs/w25n01gv.yaml \
    configs/mt29f2g01.yaml
do
    ./flash_modle_sim "$profile" --validate-only > /tmp/flash_modle_profile.out
    grep -q "Validation Result: PASS" /tmp/flash_modle_profile.out
done

if ./flash_modle_sim configs/invalid_nor_with_nand_command.yaml --validate-only > /tmp/flash_modle_invalid.out 2>&1; then
    echo "invalid config unexpectedly passed validation" >&2
    exit 1
fi
grep -q "NOR config enables SPI-NAND-only commands" /tmp/flash_modle_invalid.out

echo "FLASH_MODLE smoke tests PASS"
