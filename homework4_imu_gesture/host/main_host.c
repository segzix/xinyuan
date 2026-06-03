#include "imu_dataset_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_gesture(uint32_t time_ms, GestureResult result, void *user_data)
{
    (void)user_data;
    if (result == GESTURE_OTHER) {
        return;
    }
    printf("%ums, %s\n", (unsigned int)time_ms, gesture_result_name(result));
}

static void print_crc(uint8_t crc, void *user_data)
{
    (void)user_data;
    printf("CRC8_SMBUS(320000B)=0x%02X\n", crc);
}

static void print_crc_pending(uint32_t bytes_read, uint32_t target_bytes, void *user_data)
{
    (void)user_data;
    printf("CRC8_SMBUS pending: only %u/%u bytes read\n",
           (unsigned int)bytes_read,
           (unsigned int)target_bytes);
}

static int parse_line_value(const char *line, int16_t *value)
{
    char *end = NULL;
    long parsed;

    parsed = strtol(line, &end, 10);
    if (end == line) {
        return 0;
    }
    if (parsed < -32768L) {
        parsed = -32768L;
    } else if (parsed > 32767L) {
        parsed = 32767L;
    }

    *value = (int16_t)parsed;
    return 1;
}

static int process_file(const char *path)
{
    FILE *fp;
    ImuDatasetRunner runner;
    ImuGyroAccelData sample;
    int16_t values[IMU_GESTURE_CHANNELS];
    int has_data = 0;
    uint8_t col = 0u;
    char line[128];

    fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }

    imu_dataset_runner_init(&runner,
                            print_gesture,
                            NULL,
                            print_crc,
                            print_crc_pending,
                            NULL);

    while (fgets(line, sizeof(line), fp) != NULL) {
        int16_t value;

        if (!has_data) {
            if (strstr(line, "TYPE") != NULL) {
                has_data = 1;
            }
            continue;
        }

        if (!parse_line_value(line, &value)) {
            continue;
        }

        values[col++] = value;
        if (col != IMU_GESTURE_CHANNELS) {
            continue;
        }

        sample.gx = values[0];
        sample.gy = values[1];
        sample.gz = values[2];
        sample.ax = values[3];
        sample.ay = values[4];
        sample.az = values[5];
        sample.debug = values[6];
        col = 0u;

        imu_dataset_runner_process_sample(&runner, &sample);
    }

    imu_dataset_runner_finish(&runner);

    fclose(fp);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s /path/to/imu_dataset.txt\n", argv[0]);
        return 1;
    }

    return process_file(argv[1]);
}
