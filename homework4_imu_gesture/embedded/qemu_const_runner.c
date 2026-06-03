/*
 * QEMU SDK dataset entry that does not use fopen.
 *
 * Convert selected VeriHealthi_IMU_Dataset txt files into const
 * ImuGyroAccelData arrays and call imu_gesture_run_const_dataset() from the SDK
 * application task. Keep those arrays const so they live in read-only memory.
 */
#include "imu_qemu_const_runner.h"

#include "imu_dataset_runner.h"

#include <stddef.h>
#include <stdint.h>
#include "uart_printf.h"

static void print_gesture(uint32_t time_ms, GestureResult result, void *user_data)
{
    (void)user_data;
    if (result == GESTURE_OTHER) {
        return;
    }
    uart_printf("%ums, %s\r\n", (unsigned int)time_ms, gesture_result_name(result));
}

static void print_crc(uint8_t crc, void *user_data)
{
    (void)user_data;
    uart_printf("CRC8_SMBUS(320000B)=0x%02X\r\n", crc);
}

static void print_crc_pending(uint32_t bytes_read, uint32_t target_bytes, void *user_data)
{
    (void)user_data;
    uart_printf("CRC8_SMBUS pending: only %u/%u bytes read\r\n",
                (unsigned int)bytes_read,
                (unsigned int)target_bytes);
}

void imu_gesture_run_const_dataset(const ImuGyroAccelData *samples, uint32_t sample_count)
{
    static ImuDatasetRunner runner;
    uint32_t i;

    imu_dataset_runner_init(&runner,
                            print_gesture,
                            NULL,
                            print_crc,
                            print_crc_pending,
                            NULL);

    for (i = 0u; i < sample_count; i++) {
        imu_dataset_runner_process_sample(&runner, &samples[i]);
    }
    imu_dataset_runner_finish(&runner);
}
