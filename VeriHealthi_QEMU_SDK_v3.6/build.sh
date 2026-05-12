#!/bin/bash
# VeriHealthi QEMU SDK 编译脚本
set -e
SDK_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SDK_DIR"
mkdir -p build

F="-march=rv32imafc_xxldsp -mabi=ilp32f -mcmodel=medlow -O2 -g -Wall -Werror -ffunction-sections -fdata-sections -fno-common"
I="-Igalaxy_sdk -Igalaxy_sdk/bsp/include/arch/riscv/n309 -Igalaxy_sdk/bsp/include -Igalaxy_sdk/config/include -Igalaxy_sdk/drivers/include -Igalaxy_sdk/modules/external/riscv_dsp/include -Igalaxy_sdk/modules/external/riscv_dsp/PrivateInclude -Igalaxy_sdk/modules/include -Igalaxy_sdk/os/include -Igalaxy_sdk/osal/include"

echo "==> Compiling..."
riscv64-unknown-elf-gcc -c $F $I galaxy_sdk/main.c -o build/main.o
riscv64-unknown-elf-gcc -c $F $I galaxy_sdk/bsp/src/qemu_board.c -o build/qemu_board.o
riscv64-unknown-elf-gcc -c $F -x assembler-with-cpp $I galaxy_sdk/bsp/src/startup_riscv.S -o build/startup.o
riscv64-unknown-elf-gcc -c $F -x assembler-with-cpp $I galaxy_sdk/bsp/src/intexc_riscv.S -o build/intexc.o
riscv64-unknown-elf-gcc -c $F -x assembler-with-cpp $I galaxy_sdk/bsp/src/portasm.S -o build/portasm.o

echo "==> Linking..."
riscv64-unknown-elf-g++ $F \
  -nostartfiles -T galaxy_sdk/n309_iot_qemu.ld \
  -Wl,--gc-sections -Wl,--no-warn-rwx-segments \
  -Lgalaxy_sdk/bsp/lib -Lgalaxy_sdk/drivers/lib -Lgalaxy_sdk/modules/lib \
  -Lgalaxy_sdk/modules/external/riscv_dsp -Lgalaxy_sdk/os/lib -Lgalaxy_sdk/osal/lib \
  build/main.o build/qemu_board.o build/startup.o build/intexc.o build/portasm.o \
  -Wl,--start-group \
    -lstdc++ -lbsp_riscv -ldriver_riscv -lcommon_riscv \
    -losal_riscv -los_riscv -lnmsis_dsp_rv32imafc_xxldsp \
    -lc_nano -lgcc -lsemihost \
  -Wl,--end-group \
  -o build/qemu.out

echo "==> Done: build/qemu.out"
riscv64-unknown-elf-size build/qemu.out
