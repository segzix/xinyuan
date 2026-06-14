#ifndef IMU_BUFFER_H
#define IMU_BUFFER_H

#include <stdbool.h>
#include "imu_gesture_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IMU_BUFFER_BLOCK_SAMPLES 64u
#define IMU_BUFFER_BLOCK_COUNT 2u

typedef struct ImuBufferBlock {
    ImuGyroAccelData samples[IMU_BUFFER_BLOCK_SAMPLES];
    ImuBlock block;
    uint16_t fill_count;
    bool ready;
} ImuBufferBlock;

typedef struct ImuBuffer {
    ImuBufferBlock blocks[IMU_BUFFER_BLOCK_COUNT];
    uint8_t write_index;
    uint32_t overflow_count;
} ImuBuffer;

/* Single producer, single consumer. Do not call push/release concurrently. */
void imu_buffer_init(ImuBuffer *buffer);
bool imu_buffer_push(ImuBuffer *buffer, const ImuGyroAccelData *sample, ImuBlock *ready_block);
bool imu_buffer_flush(ImuBuffer *buffer, ImuBlock *ready_block);
ImuBlock *imu_buffer_block_descriptor(ImuBuffer *buffer, const ImuBlock *block);
void imu_buffer_release(ImuBuffer *buffer, const ImuBlock *block);
uint32_t imu_buffer_overflow_count(const ImuBuffer *buffer);

#ifdef __cplusplus
}
#endif

#endif
