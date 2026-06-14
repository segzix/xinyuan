#include "imu_buffer.h"

#include <string.h>

void imu_buffer_init(ImuBuffer *buffer)
{
    if (buffer != NULL) {
        memset(buffer, 0, sizeof(*buffer));
    }
}

static void imu_buffer_prepare_ready_block(ImuBufferBlock *block)
{
    block->ready = true;
    block->block.samples = block->samples;
    block->block.count = block->fill_count;
    block->block.byte_count = (uint32_t)block->fill_count * IMU_GESTURE_SAMPLE_BYTES;
}

static void imu_buffer_reset_block(ImuBufferBlock *block)
{
    block->fill_count = 0u;
    block->ready = false;
    block->block.samples = NULL;
    block->block.count = 0u;
    block->block.byte_count = 0u;
}

bool imu_buffer_push(ImuBuffer *buffer, const ImuGyroAccelData *sample, ImuBlock *ready_block)
{
    ImuBufferBlock *block;

    if (buffer == NULL || sample == NULL) {
        return false;
    }

    block = &buffer->blocks[buffer->write_index];
    if (block->ready) {
        buffer->overflow_count++;
        return false;
    }

    block->samples[block->fill_count++] = *sample;
    if (block->fill_count < IMU_BUFFER_BLOCK_SAMPLES) {
        return false;
    }

    imu_buffer_prepare_ready_block(block);
    if (ready_block != NULL) {
        *ready_block = block->block;
    }

    buffer->write_index = (uint8_t)((buffer->write_index + 1u) % IMU_BUFFER_BLOCK_COUNT);
    return true;
}

bool imu_buffer_flush(ImuBuffer *buffer, ImuBlock *ready_block)
{
    ImuBufferBlock *block;

    if (buffer == NULL || ready_block == NULL) {
        return false;
    }

    block = &buffer->blocks[buffer->write_index];
    if (block->ready || block->fill_count == 0u) {
        return false;
    }

    imu_buffer_prepare_ready_block(block);
    *ready_block = block->block;
    return true;
}

ImuBlock *imu_buffer_block_descriptor(ImuBuffer *buffer, const ImuBlock *block)
{
    uint8_t i;

    if (buffer == NULL || block == NULL) {
        return NULL;
    }

    for (i = 0u; i < IMU_BUFFER_BLOCK_COUNT; i++) {
        ImuBufferBlock *candidate = &buffer->blocks[i];

        if (candidate->samples == block->samples) {
            return &candidate->block;
        }
    }

    return NULL;
}

void imu_buffer_release(ImuBuffer *buffer, const ImuBlock *block)
{
    uint8_t i;

    if (buffer == NULL || block == NULL) {
        return;
    }

    for (i = 0u; i < IMU_BUFFER_BLOCK_COUNT; i++) {
        ImuBufferBlock *candidate = &buffer->blocks[i];

        if (candidate->samples == block->samples) {
            imu_buffer_reset_block(candidate);
            return;
        }
    }
}

uint32_t imu_buffer_overflow_count(const ImuBuffer *buffer)
{
    return buffer == NULL ? 0u : buffer->overflow_count;
}
