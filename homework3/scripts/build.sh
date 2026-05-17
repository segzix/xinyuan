#!/bin/bash
# StepCounter Nuclei RISC-V QEMU 编译脚本
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SDK_DIR="$(cd "$ROOT_DIR/../VeriHealthi_QEMU_SDK_v3.6" && pwd)"
BUILD_DIR="$ROOT_DIR/build"

if [ ! -d "$SDK_DIR" ]; then
    echo "ERROR: SDK not found at $SDK_DIR"
    exit 1
fi

mkdir -p "$BUILD_DIR"

F="-march=rv32imafc -mabi=ilp32f -mcmodel=medlow -O2 -g -Wall -Werror -ffunction-sections -fdata-sections -fno-common"
D="-DMAX_SAMPLES=3000 -DMAX_PEAKS=500"
I="-I$ROOT_DIR/C -I$SDK_DIR/galaxy_sdk -I$SDK_DIR/galaxy_sdk/bsp/include/arch/riscv/n309 -I$SDK_DIR/galaxy_sdk/bsp/include -I$SDK_DIR/galaxy_sdk/config/include -I$SDK_DIR/galaxy_sdk/drivers/include -I$SDK_DIR/galaxy_sdk/modules/external/riscv_dsp/include -I$SDK_DIR/galaxy_sdk/modules/external/riscv_dsp/PrivateInclude -I$SDK_DIR/galaxy_sdk/modules/include -I$SDK_DIR/galaxy_sdk/os/include -I$SDK_DIR/galaxy_sdk/osal/include"

echo "==> SDK: $SDK_DIR"
echo "==> Compiling..."

riscv64-unknown-elf-gcc -c $F $D $I "$ROOT_DIR/C/main_qemu.c"                     -o "$BUILD_DIR/main_qemu.o"
riscv64-unknown-elf-gcc -c $F $D $I "$ROOT_DIR/C/alg_step_counter_improved.c"   -o "$BUILD_DIR/alg_step_counter_improved.o"
riscv64-unknown-elf-gcc -c $F      $I "$SDK_DIR/galaxy_sdk/bsp/src/qemu_board.c"  -o "$BUILD_DIR/qemu_board.o"
riscv64-unknown-elf-gcc -c $F -x assembler-with-cpp $I "$SDK_DIR/galaxy_sdk/bsp/src/startup_riscv.S" -o "$BUILD_DIR/startup.o"
riscv64-unknown-elf-gcc -c $F -x assembler-with-cpp $I "$SDK_DIR/galaxy_sdk/bsp/src/intexc_riscv.S"  -o "$BUILD_DIR/intexc.o"
riscv64-unknown-elf-gcc -c $F -x assembler-with-cpp $I "$SDK_DIR/galaxy_sdk/bsp/src/portasm.S"       -o "$BUILD_DIR/portasm.o"

echo "==> Linking..."
riscv64-unknown-elf-g++ $F \
  -nostartfiles -T "$SDK_DIR/galaxy_sdk/n309_iot_qemu.ld" \
  -Wl,--gc-sections -Wl,--no-warn-rwx-segments \
  -L"$SDK_DIR/galaxy_sdk/bsp/lib" -L"$SDK_DIR/galaxy_sdk/drivers/lib" -L"$SDK_DIR/galaxy_sdk/modules/lib" \
  -L"$SDK_DIR/galaxy_sdk/modules/external/riscv_dsp" -L"$SDK_DIR/galaxy_sdk/os/lib" -L"$SDK_DIR/galaxy_sdk/osal/lib" \
  "$BUILD_DIR/main_qemu.o" "$BUILD_DIR/alg_step_counter_improved.o" \
  "$BUILD_DIR/qemu_board.o" "$BUILD_DIR/startup.o" "$BUILD_DIR/intexc.o" "$BUILD_DIR/portasm.o" \
  -Wl,--start-group \
    -lstdc++ -lbsp_riscv -ldriver_riscv -lcommon_riscv \
    -losal_riscv -los_riscv -lnmsis_dsp_rv32imafc_xxldsp \
    -lc_nano -lgcc -lsemihost \
  -Wl,--end-group \
  -o "$BUILD_DIR/step_counter.out"

echo ""
echo "==> Done: $BUILD_DIR/step_counter.out"
riscv64-unknown-elf-size "$BUILD_DIR/step_counter.out"
