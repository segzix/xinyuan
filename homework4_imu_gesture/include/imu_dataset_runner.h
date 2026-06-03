#ifndef IMU_DATASET_RUNNER_H
#define IMU_DATASET_RUNNER_H

#include "algo_manager.h"
#include "imu_buffer.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ImuRunnerCrcCallback)(uint8_t crc, void *user_data);
typedef void (*ImuRunnerCrcPendingCallback)(uint32_t bytes_read,
                                           uint32_t target_bytes,
                                           void *user_data);

typedef struct ImuDatasetRunner {
    AlgoManager manager;
    ImuBuffer buffer;
    uint8_t crc;
    uint32_t crc_bytes;
    bool crc_printed;
    ImuRunnerCrcCallback crc_callback;
    ImuRunnerCrcPendingCallback crc_pending_callback;
    void *crc_user_data;
} ImuDatasetRunner;

void imu_dataset_runner_init(ImuDatasetRunner *runner,
                             AlgoGestureCallback gesture_callback,
                             void *gesture_user_data,
                             ImuRunnerCrcCallback crc_callback,
                             ImuRunnerCrcPendingCallback crc_pending_callback,
                             void *crc_user_data);
void imu_dataset_runner_process_sample(ImuDatasetRunner *runner, const ImuGyroAccelData *sample);
void imu_dataset_runner_finish(ImuDatasetRunner *runner);
uint32_t imu_dataset_runner_crc_bytes_read(const ImuDatasetRunner *runner);

#ifdef __cplusplus
}
#endif

#endif
