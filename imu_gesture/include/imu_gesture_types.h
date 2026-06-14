#ifndef IMU_GESTURE_TYPES_H
#define IMU_GESTURE_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IMU_GESTURE_SAMPLE_RATE_HZ 50u
#define IMU_GESTURE_CHANNELS 7u
#define IMU_GESTURE_CRC_TARGET_BYTES 320000u

typedef struct ImuGyroAccelData {
    int16_t gx;
    int16_t gy;
    int16_t gz;
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t debug;
} ImuGyroAccelData;

typedef char ImuGyroAccelData_must_be_14_bytes[(sizeof(ImuGyroAccelData) == 14u) ? 1 : -1];

#define IMU_GESTURE_SAMPLE_BYTES ((uint32_t)sizeof(ImuGyroAccelData))

typedef enum GestureResult {
    GESTURE_OTHER = 0,
    GESTURE_PINCH,
    GESTURE_CLENCH,
    GESTURE_UP,
    GESTURE_DOWN,
} GestureResult;

typedef struct ImuBlock {
    ImuGyroAccelData *samples;
    uint16_t count;
    uint32_t byte_count;
} ImuBlock;

#ifdef __cplusplus
}
#endif

#endif
