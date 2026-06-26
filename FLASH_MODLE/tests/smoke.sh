#!/usr/bin/env sh
set -eu

cd "$(dirname "$0")/.."

./flash_model_sim configs/demo_nor.yaml --self-test > /tmp/flash_model_nor.out
grep -q "Test Result: PASS" /tmp/flash_model_nor.out
grep -q "Unique ID" /tmp/flash_model_nor.out
grep -q "SFDP" /tmp/flash_model_nor.out
grep -q "NOR page wrap head" /tmp/flash_model_nor.out
grep -q "NOR_BLOCK32_ERASE accepted" /tmp/flash_model_nor.out
grep -q "NOR_BLOCK_ERASE accepted" /tmp/flash_model_nor.out
grep -q "NOR_CHIP_ERASE accepted" /tmp/flash_model_nor.out
grep -q "deep power-down rejects normal read" /tmp/flash_model_nor.out
grep -q "SUSPEND" /tmp/flash_model_nor.out
grep -q "RESUME" /tmp/flash_model_nor.out
grep -q "NOR security read" /tmp/flash_model_nor.out
grep -q "NOR security old&new" /tmp/flash_model_nor.out
grep -q "NOR security register lock rejects program" /tmp/flash_model_nor.out

./flash_model_sim configs/demo_nor_bp.yaml --self-test > /tmp/flash_model_nor_bp.out
grep -q "Test Result: PASS" /tmp/flash_model_nor_bp.out
grep -q "NOR block protect rejects erase in protected range" /tmp/flash_model_nor_bp.out
grep -q "NOR block protect rejects program in protected range" /tmp/flash_model_nor_bp.out

./flash_model_sim configs/demo_nand.yaml --self-test > /tmp/flash_model_nand.out
grep -q "Test Result: PASS" /tmp/flash_model_nand.out
grep -q "block protect rejects erase before unlock" /tmp/flash_model_nand.out
grep -q "NAND OTP read" /tmp/flash_model_nand.out
grep -q "NAND OTP mode rejects block erase" /tmp/flash_model_nand.out
grep -q "ECC reserved spare range rejects program" /tmp/flash_model_nand.out
grep -q "strict sequential program rejects skipped page" /tmp/flash_model_nand.out
grep -q "COPY_BACK accepted" /tmp/flash_model_nand.out
grep -q "bad block management rejects erase" /tmp/flash_model_nand.out
grep -q "bad block management rejects program" /tmp/flash_model_nand.out

for profile in \
    configs/nor_by25q64as.yaml \
    configs/nor_gd25le128e.yaml \
    configs/nand_gd5f1gm7ue.yaml \
    configs/nor_m25p40.yaml \
    configs/nor_mx25l25645g.yaml \
    configs/nor_w25q32jv.yaml \
    configs/nand_w25n01gv.yaml \
    configs/nand_mt29f2g01.yaml
do
    ./flash_model_sim "$profile" --validate-only > /tmp/flash_model_profile.out
    grep -q "Validation Result: PASS" /tmp/flash_model_profile.out
done

if ./flash_model_sim configs/invalid_nor_nand_cmd.yaml --validate-only > /tmp/flash_model_invalid.out 2>&1; then
    echo "invalid config unexpectedly passed validation" >&2
    exit 1
fi
grep -q "NOR config enables SPI-NAND-only commands" /tmp/flash_model_invalid.out

echo "FLASH_MODEL smoke tests PASS"
