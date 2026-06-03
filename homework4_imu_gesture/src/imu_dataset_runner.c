#include "imu_dataset_runner.h"

#include "crc8_smbus.h"

static void update_first_320000_bytes_crc(ImuDatasetRunner *runner, const ImuGyroAccelData *sample)
{
    uint32_t remain;
    uint32_t update;

    if (runner == NULL || runner->crc_printed || sample == NULL) {
        return;
    }

    remain = IMU_GESTURE_CRC_TARGET_BYTES - runner->crc_bytes;
    /* The required byte boundary may split the final sample. */
    update = IMU_GESTURE_SAMPLE_BYTES < remain ? IMU_GESTURE_SAMPLE_BYTES : remain;
    runner->crc = crc8_smbus_update(runner->crc, sample, update);
    runner->crc_bytes += update;

    if (runner->crc_bytes == IMU_GESTURE_CRC_TARGET_BYTES) {
        runner->crc_printed = true;
        if (runner->crc_callback != NULL) {
            runner->crc_callback(runner->crc, runner->crc_user_data);
        }
    }
}

void imu_dataset_runner_init(ImuDatasetRunner *runner,
                             AlgoGestureCallback gesture_callback,
                             void *gesture_user_data,
                             ImuRunnerCrcCallback crc_callback,
                             ImuRunnerCrcPendingCallback crc_pending_callback,
                             void *crc_user_data)
{
    if (runner == NULL) {
        return;
    }

    algo_manager_init(&runner->manager, gesture_callback, gesture_user_data);
    imu_buffer_init(&runner->buffer);
    runner->crc = 0u;
    runner->crc_bytes = 0u;
    runner->crc_printed = false;
    runner->crc_callback = crc_callback;
    runner->crc_pending_callback = crc_pending_callback;
    runner->crc_user_data = crc_user_data;
}

void imu_dataset_runner_process_sample(ImuDatasetRunner *runner, const ImuGyroAccelData *sample)
{
    ImuBlock ready_block;

    if (runner == NULL || sample == NULL) {
        return;
    }

    update_first_320000_bytes_crc(runner, sample);
    if (imu_buffer_push(&runner->buffer, sample, &ready_block)) {
        algo_manager_process(&runner->manager, &ready_block);
        imu_buffer_release(&runner->buffer, &ready_block);
    }
}

void imu_dataset_runner_finish(ImuDatasetRunner *runner)
{
    ImuBlock ready_block;

    if (runner == NULL) {
        return;
    }

    if (imu_buffer_flush(&runner->buffer, &ready_block)) {
        algo_manager_process(&runner->manager, &ready_block);
        imu_buffer_release(&runner->buffer, &ready_block);
    }

    if (!runner->crc_printed && runner->crc_pending_callback != NULL) {
        runner->crc_pending_callback(runner->crc_bytes,
                                     IMU_GESTURE_CRC_TARGET_BYTES,
                                     runner->crc_user_data);
    }
}

uint32_t imu_dataset_runner_crc_bytes_read(const ImuDatasetRunner *runner)
{
    return runner == NULL ? 0u : runner->crc_bytes;
}
