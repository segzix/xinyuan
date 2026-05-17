#!/bin/bash
# translate_message 编译脚本 - 使用 VeriHealthi SDK
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SDK_DIR="$(cd "$SCRIPT_DIR/../VeriHealthi_QEMU_SDK_v3.6" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

mkdir -p "$BUILD_DIR"

F="-march=rv32imafc -mabi=ilp32f -mcmodel=medlow -O2 -g -Wall -Werror -ffunction-sections -fdata-sections -fno-common"
I="-I$SDK_DIR/galaxy_sdk -I$SDK_DIR/galaxy_sdk/bsp/include/arch/riscv/n309 -I$SDK_DIR/galaxy_sdk/bsp/include -I$SDK_DIR/galaxy_sdk/config/include -I$SDK_DIR/galaxy_sdk/drivers/include -I$SDK_DIR/galaxy_sdk/modules/external/riscv_dsp/include -I$SDK_DIR/galaxy_sdk/modules/external/riscv_dsp/PrivateInclude -I$SDK_DIR/galaxy_sdk/modules/include -I$SDK_DIR/galaxy_sdk/os/include -I$SDK_DIR/galaxy_sdk/osal/include"

echo "==> Compiling translate_message..."
riscv64-unknown-elf-gcc -c $F $I "$SCRIPT_DIR/main_freertos_vpi.c"             -o "$BUILD_DIR/main.o"
riscv64-unknown-elf-gcc -c $F $I "$SCRIPT_DIR/message_table.c"   -o "$BUILD_DIR/message_table.o"
riscv64-unknown-elf-gcc -c $F $I "$SCRIPT_DIR/emu_dsp.c"         -o "$BUILD_DIR/emu_dsp.o"
riscv64-unknown-elf-gcc -c $F $I "$SDK_DIR/galaxy_sdk/bsp/src/qemu_board.c" -o "$BUILD_DIR/qemu_board.o"
riscv64-unknown-elf-gcc -c $F -x assembler-with-cpp $I "$SDK_DIR/galaxy_sdk/bsp/src/startup_riscv.S" -o "$BUILD_DIR/startup.o"
riscv64-unknown-elf-gcc -c $F -x assembler-with-cpp $I "$SDK_DIR/galaxy_sdk/bsp/src/intexc_riscv.S"  -o "$BUILD_DIR/intexc.o"
riscv64-unknown-elf-gcc -c $F -x assembler-with-cpp $I "$SDK_DIR/galaxy_sdk/bsp/src/portasm.S"       -o "$BUILD_DIR/portasm.o"

echo "==> Linking translate_message..."
riscv64-unknown-elf-g++ $F \
  -nostartfiles -T "$SDK_DIR/galaxy_sdk/n309_iot_qemu.ld" \
  -Wl,--gc-sections -Wl,--no-warn-rwx-segments \
  -L"$SDK_DIR/galaxy_sdk/bsp/lib" -L"$SDK_DIR/galaxy_sdk/drivers/lib" -L"$SDK_DIR/galaxy_sdk/modules/lib" \
  -L"$SDK_DIR/galaxy_sdk/modules/external/riscv_dsp" -L"$SDK_DIR/galaxy_sdk/os/lib" -L"$SDK_DIR/galaxy_sdk/osal/lib" \
  "$BUILD_DIR/main.o" "$BUILD_DIR/message_table.o" "$BUILD_DIR/emu_dsp.o" "$BUILD_DIR/qemu_board.o" "$BUILD_DIR/startup.o" "$BUILD_DIR/intexc.o" "$BUILD_DIR/portasm.o" \
  -Wl,--start-group \
    -lstdc++ -lbsp_riscv -ldriver_riscv -lcommon_riscv \
    -losal_riscv -los_riscv -lnmsis_dsp_rv32imafc_xxldsp \
    -lc_nano -lgcc -lsemihost \
  -Wl,--end-group \
  -o "$BUILD_DIR/translate_message.out"

echo "==> Done: $BUILD_DIR/translate_message.out"
riscv64-unknown-elf-size "$BUILD_DIR/translate_message.out"
