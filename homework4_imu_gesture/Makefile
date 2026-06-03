CC ?= gcc
CFLAGS ?= -O2 -g -Wall -Wextra -Werror -std=c99
INCLUDES := -Iinclude
HOST_BIN := build/imu_gesture_host
HOST_SRC := \
	host/main_host.c \
	src/algo_manager.c \
	src/crc8_smbus.c \
	src/imu_dataset_runner.c \
	src/gesture_algo.c \
	src/imu_buffer.c

.PHONY: all clean run

all: $(HOST_BIN)

$(HOST_BIN): $(HOST_SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

run: $(HOST_BIN)
	@if [ -z "$(DATA)" ]; then \
		printf 'Usage: make run DATA=/path/to/imu.txt\n'; \
		exit 1; \
	fi
	$(HOST_BIN) $(DATA)

clean:
	rm -rf build
