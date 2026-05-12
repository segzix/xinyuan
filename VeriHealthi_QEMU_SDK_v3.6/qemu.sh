#!/bin/bash
# QEMU 终端
SDK_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SDK_DIR"
qemu-system-riscv32 \
  -M nuclei_evalsoc -cpu nuclei-n307 \
  -nographic -serial stdio -nodefaults \
  -semihosting \
  -S -gdb tcp::1234 \
  -kernel build/qemu.out
