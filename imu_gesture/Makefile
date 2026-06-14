CC ?= gcc
CFLAGS ?= -O2 -g -Wall -Wextra -Werror -std=c99
PYTHON ?= python3
PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
INCLUDES := -I$(PROJECT_ROOT)/include
HOST_BIN := $(PROJECT_ROOT)/build/imu_gesture_host
DATA_DIR ?= $(PROJECT_ROOT)/../VeriHealthi_IMU_Dataset
SIM_DIR ?= $(PROJECT_ROOT)/build/simulated_dataset
HOST_SRC := \
	$(PROJECT_ROOT)/host/main_host.c \
	$(PROJECT_ROOT)/src/algo_manager.c \
	$(PROJECT_ROOT)/src/crc8_smbus.c \
	$(PROJECT_ROOT)/src/imu_dataset_runner.c \
	$(PROJECT_ROOT)/src/gesture_algo.c \
	$(PROJECT_ROOT)/src/imu_buffer.c
SIM_SCRIPT := $(PROJECT_ROOT)/sim/simulate_gestures.py
PY_ALGO_SIM_SCRIPT := $(PROJECT_ROOT)/sim/gesture_algo_sim.py

.PHONY: all help clean run run-all eval sim sim-run sim-check py-algo py-algo-compare py-algo-compare-sim py-algo-compare-all py-algo-compare-all-verbose

all: $(HOST_BIN)

help:
	@printf 'IMU gesture host verification commands:\n'
	@printf '  make                         Build build/imu_gesture_host\n'
	@printf '  make run DATA=/path/file.txt Run one IMU text file\n'
	@printf '  make run-all                 Run all txt files under DATA_DIR\n'
	@printf '  make eval                    Evaluate DATA_DIR by class directories\n'
	@printf '  make sim                     Generate simulated dataset into SIM_DIR\n'
	@printf '  make sim-run                 Generate simulated data and run host output\n'
	@printf '  make sim-check               Generate simulated data and verify expected labels\n'
	@printf '  make py-algo DATA=/path/file.txt\n'
	@printf '                               Run the Python mirror of gesture_algo.c\n'
	@printf '  make py-algo-compare DATA=/path/file.txt\n'
	@printf '                               Compare Python mirror output with C host output\n'
	@printf '  make py-algo-compare-sim     Compare Python mirror with C host on SIM_DIR\n'
	@printf '  make py-algo-compare-all     Compare Python mirror with C host on DATA_DIR, summary with class stats\n'
	@printf '  make py-algo-compare-all-verbose\n'
	@printf '                               Compare Python mirror with C host on DATA_DIR, per-file output\n'
	@printf '  make eval DATA_DIR=build/simulated_dataset\n'
	@printf '                               Evaluate generated simulated dataset\n'
	@printf '\nDefaults:\n'
	@printf '  DATA_DIR=%s\n' "$(DATA_DIR)"
	@printf '  SIM_DIR=%s\n' "$(SIM_DIR)"
	@printf '  PYTHON=%s\n' "$(PYTHON)"

$(HOST_BIN): $(HOST_SRC)
	@mkdir -p $(PROJECT_ROOT)/build
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

run: $(HOST_BIN)
	@if [ -z "$(DATA)" ]; then \
		printf 'Usage: make run DATA=/path/to/imu.txt\n'; \
		exit 1; \
	fi
	$(HOST_BIN) $(DATA)

run-all: $(HOST_BIN)
	find "$(DATA_DIR)" -type f -name '*.txt' ! -name 'ReadMe.txt' | sort | while IFS= read -r f; do \
		printf '\n== %s ==\n' "$$f"; \
		$(HOST_BIN) "$$f"; \
	done

eval: $(HOST_BIN)
	@for cls in pinch clench up down others; do \
		dir="$(DATA_DIR)/$$cls"; \
		total=0; correct=0; miss_or_fp=0; wrong_files=0; wrong_lines=0; accepted_pair_files=0; accepted_pair_lines=0; \
		accepted_pair=""; \
		if [ "$$cls" = "up" ]; then accepted_pair="down"; fi; \
		if [ "$$cls" = "down" ]; then accepted_pair="up"; fi; \
		if [ ! -d "$$dir" ]; then \
			printf '%-7s missing directory: %s\n' "$$cls" "$$dir"; \
			continue; \
		fi; \
		for f in "$$dir"/*.txt; do \
			[ -f "$$f" ] || continue; \
			total=$$((total + 1)); \
			out="$$($(HOST_BIN) "$$f" | awk -F', ' '/, / && $$2 ~ /^(pinch|clench|up|down)$$/ {print $$2}')"; \
			if [ "$$cls" = "others" ]; then \
				if [ -z "$$out" ]; then \
					correct=$$((correct + 1)); \
				else \
					miss_or_fp=$$((miss_or_fp + 1)); \
					wrong_files=$$((wrong_files + 1)); \
					lines=$$(printf '%s\n' "$$out" | sed '/^$$/d' | wc -l); \
					wrong_lines=$$((wrong_lines + lines)); \
				fi; \
			else \
				if printf '%s\n' "$$out" | grep -qx "$$cls"; then \
					correct=$$((correct + 1)); \
				else \
					miss_or_fp=$$((miss_or_fp + 1)); \
				fi; \
				paired=$$(printf '%s\n' "$$out" | awk -v a="$$accepted_pair" 'a != "" && NF && $$0 == a {n++} END {print n+0}'); \
				if [ "$$paired" -gt 0 ]; then \
					accepted_pair_files=$$((accepted_pair_files + 1)); \
					accepted_pair_lines=$$((accepted_pair_lines + paired)); \
				fi; \
				wrong=$$(printf '%s\n' "$$out" | awk -v c="$$cls" -v a="$$accepted_pair" 'NF && $$0 != c && (a == "" || $$0 != a) {n++} END {print n+0}'); \
				if [ "$$wrong" -gt 0 ]; then \
					wrong_files=$$((wrong_files + 1)); \
					wrong_lines=$$((wrong_lines + wrong)); \
				fi; \
			fi; \
		done; \
		printf '%-7s files=%d correct_files=%d miss_or_fp_files=%d wrong_files=%d wrong_lines=%d accepted_pair_files=%d accepted_pair_lines=%d\n' \
			"$$cls" "$$total" "$$correct" "$$miss_or_fp" "$$wrong_files" "$$wrong_lines" "$$accepted_pair_files" "$$accepted_pair_lines"; \
	done

sim:
	$(PYTHON) $(SIM_SCRIPT) --output-dir "$(SIM_DIR)"

sim-run: $(HOST_BIN)
	$(PYTHON) $(SIM_SCRIPT) --output-dir "$(SIM_DIR)" --host-bin "$(HOST_BIN)" --run-host

sim-check: $(HOST_BIN)
	$(PYTHON) $(SIM_SCRIPT) --output-dir "$(SIM_DIR)" --host-bin "$(HOST_BIN)" --check

py-algo:
	@if [ -z "$(DATA)" ]; then \
		printf 'Usage: make py-algo DATA=/path/to/imu.txt\n'; \
		exit 1; \
	fi
	$(PYTHON) $(PY_ALGO_SIM_SCRIPT) $(DATA)

py-algo-compare: $(HOST_BIN)
	@if [ -z "$(DATA)" ]; then \
		printf 'Usage: make py-algo-compare DATA=/path/to/imu.txt\n'; \
		exit 1; \
	fi
	$(PYTHON) $(PY_ALGO_SIM_SCRIPT) --host-bin "$(HOST_BIN)" --compare-host $(DATA)

py-algo-compare-sim: $(HOST_BIN) sim
	$(PYTHON) $(PY_ALGO_SIM_SCRIPT) --dataset-dir "$(SIM_DIR)" --host-bin "$(HOST_BIN)" --compare-host

py-algo-compare-all: $(HOST_BIN)
	@$(PYTHON) $(PY_ALGO_SIM_SCRIPT) --dataset-dir "$(DATA_DIR)" --host-bin "$(HOST_BIN)" --compare-host --quiet

py-algo-compare-all-verbose: $(HOST_BIN)
	$(PYTHON) $(PY_ALGO_SIM_SCRIPT) --dataset-dir "$(DATA_DIR)" --host-bin "$(HOST_BIN)" --compare-host

clean:
	rm -rf $(PROJECT_ROOT)/build
