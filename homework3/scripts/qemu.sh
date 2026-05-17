#!/bin/bash
# StepCounter QEMU 运行脚本 (Nuclei N307)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "==> Running step counter on QEMU..."
echo ""

qemu-system-riscv32 \
  -M nuclei_evalsoc -cpu nuclei-n307 \
  -nographic -serial stdio -nodefaults -semihosting \
  -kernel "$ROOT_DIR/build/step_counter.out"
