#!/bin/bash
# translate_message QEMU 运行脚本
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BINARY="$SCRIPT_DIR/build/translate_message.out"

[ -f "$BINARY" ] || { echo "Binary not found. Run build.sh first."; exit 1; }

cd "$SCRIPT_DIR"
qemu-system-riscv32 \
  -M nuclei_evalsoc -cpu nuclei-n307 \
  -nographic -serial stdio -nodefaults \
  -semihosting \
  -kernel "$BINARY"
