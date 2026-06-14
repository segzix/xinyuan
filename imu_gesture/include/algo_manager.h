#ifndef ALGO_MANAGER_H
#define ALGO_MANAGER_H

#include "gesture_algo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*AlgoGestureCallback)(uint32_t time_ms, GestureResult result, void *user_data);

typedef struct AlgoManager {
    GestureAlgoContext algo;
    uint32_t processed_bytes;
    AlgoGestureCallback callback;
    void *user_data;
} AlgoManager;

void algo_manager_init(AlgoManager *manager, AlgoGestureCallback callback, void *user_data);
void algo_manager_process(AlgoManager *manager, const ImuBlock *block);
uint32_t algo_manager_processed_time_ms(const AlgoManager *manager);

#ifdef __cplusplus
}
#endif

#endif
