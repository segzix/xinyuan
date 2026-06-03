#include "algo_manager.h"

void algo_manager_init(AlgoManager *manager, AlgoGestureCallback callback, void *user_data)
{
    if (manager == NULL) {
        return;
    }

    gesture_algo_init(&manager->algo);
    manager->processed_bytes = 0u;
    manager->callback = callback;
    manager->user_data = user_data;
}

uint32_t algo_manager_processed_time_ms(const AlgoManager *manager)
{
    uint32_t sample_count;

    if (manager == NULL) {
        return 0u;
    }

    /* Elapsed stream time after all complete samples in processed_bytes. */
    sample_count = manager->processed_bytes / IMU_GESTURE_SAMPLE_BYTES;
    return (sample_count / IMU_GESTURE_SAMPLE_RATE_HZ) * 1000u
        + ((sample_count % IMU_GESTURE_SAMPLE_RATE_HZ) * 1000u) / IMU_GESTURE_SAMPLE_RATE_HZ;
}

void algo_manager_process(AlgoManager *manager, const ImuBlock *block)
{
    GestureResult result;

    if (manager == NULL || block == NULL || block->samples == NULL || block->count == 0u) {
        return;
    }

    result = gesture_algo_process(&manager->algo, block->samples, block->count);
    manager->processed_bytes += block->byte_count;

    if (result != GESTURE_OTHER && manager->callback != NULL) {
        manager->callback(algo_manager_processed_time_ms(manager), result, manager->user_data);
    }
}
