CC       = gcc
CFLAGS   = -O2 -g -Wall -Werror
INCLUDES = -I./C
DEFINES  = -DMAX_SAMPLES=50000 -DMAX_PEAKS=5000
LDFLAGS  = -lm

.PHONY: all clean run-improved run-python run-simulation \
        qemu-build qemu-run qemu

# ============================================================
# Host build & run
# ============================================================

all: build/main_improved

build/main_improved: host/main_improved.c C/alg_step_counter_improved.c
	@mkdir -p build
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -o $@ $^ $(LDFLAGS)

run-improved: build/main_improved
	./build/main_improved

# ============================================================
# QEMU build & run
# ============================================================

qemu-build:
	bash scripts/build.sh

qemu-run:
	bash scripts/qemu.sh

qemu: qemu-build qemu-run

# ============================================================
# Python
# ============================================================

run-python:
	python3 Python/step_counter_improved.py

run-simulation:
	python3 Python/step_counter_simulation.py

# ============================================================
# Clean
# ============================================================

clean:
	rm -rf build/main_improved build/*.o build/step_counter.out
