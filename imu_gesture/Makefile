CC ?= gcc
CFLAGS ?= -O2 -g -Wall -Wextra -Werror -std=c99
PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
INCLUDES := -I$(PROJECT_ROOT)/include
HOST_BIN := $(PROJECT_ROOT)/build/imu_gesture_host
DATA_DIR ?= $(PROJECT_ROOT)/../VeriHealthi_IMU_Dataset
HOST_SRC := \
	$(PROJECT_ROOT)/host/main_host.c \
	$(PROJECT_ROOT)/src/algo_manager.c \
	$(PROJECT_ROOT)/src/crc8_smbus.c \
	$(PROJECT_ROOT)/src/imu_dataset_runner.c \
	$(PROJECT_ROOT)/src/gesture_algo.c \
	$(PROJECT_ROOT)/src/imu_buffer.c

.PHONY: all clean run run-all eval

all: $(HOST_BIN)

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

clean:
	rm -rf $(PROJECT_ROOT)/build
