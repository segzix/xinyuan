#ifndef IMU_QEMU_CONST_RUNNER_H
#define IMU_QEMU_CONST_RUNNER_H

#include "imu_gesture_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void imu_gesture_run_const_dataset(const ImuGyroAccelData *samples, uint32_t sample_count);

#ifdef __cplusplus
}
#endif

#endif
