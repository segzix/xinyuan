# translate_message

RISC-V bare-metal message translation demo running on Nuclei N307 (QEMU).

## Quick Start

```bash
cd translate_message

# 1. Build
./build.sh

# 2. Run (requires qemu-system-riscv32)
./run_qemu.sh
```

## Expected Output

```
soc init done
DSP emulation enabled
=== Message Translation Demo ===
[00] 0x0001 -> HEARTBEAT
[00] 0x0002 -> TEMPERATURE
...
=== Message Translation Demo Complete ===
```

5 rounds of 30 health/IoT message codes translated to human-readable names.

## Prerequisites

- `riscv64-unknown-elf-gcc` toolchain (Nuclei GCC)
- `qemu-system-riscv32` with Nuclei `nuclei_evalsoc` machine support
- `../VeriHealthi_QEMU_SDK_v3.6` — VeriHealthi SDK (precompiled libraries + linker script)

## Architecture

```
main.c          Bare-metal loop + 30-entry message code -> name translation table
emu_dsp.c       maddr32 DSP instruction software emulation (assembly trampoline + C logic)
build.sh        Compile against ../VeriHealthi_QEMU_SDK_v3.6 (march=rv32imafc, no DSP)
run_qemu.sh     QEMU invocation (nuclei_evalsoc, nuclei-n307)
```

Built as bare-metal with software DSP emulation.  The `maddr32` DSP instruction
is not supported by QEMU 2024.06, so `emu_dsp.c` intercepts machine exception 2
(illegal instruction) and performs the multiply-accumulate in software.

## FreeRTOS+VPI Version (experimental)

A multi-task version using the original project architecture (FreeRTOS tasks +
VPI event system) is available on the `freertos-vpi` branch/tag.  It is functional
but blocked by a QEMU limitation:

- QEMU 2024.06 supports only **partial** Nuclei `xxldsp` DSP extension
- SDK precompiled libraries contain `maddr32` instructions not supported by this QEMU
- While `emu_dsp.c` correctly emulates all `maddr32` encodings (including
  funct3=5 implicit-destination variants), FreeRTOS context switches occurring
  inside interrupt handlers create nested exception scenarios that the
  software emulation cannot resolve

The bare-metal version avoids FreeRTOS/VPI entirely and runs without issues.

## Files Not Modified

All existing files under `VeriHealthi_QEMU_SDK_v3.6/` and
`translate_message/Release/` are unchanged.
