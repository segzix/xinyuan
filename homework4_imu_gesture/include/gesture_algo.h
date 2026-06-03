#ifndef GESTURE_ALGO_H
#define GESTURE_ALGO_H

#include "imu_gesture_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GestureAlgoContext {
    ImuGyroAccelData window[100];
    uint16_t window_count;
    uint32_t cooldown_samples;
    uint16_t eval_countdown;
} GestureAlgoContext;

void gesture_algo_init(GestureAlgoContext *ctx);
GestureResult gesture_algo_process(GestureAlgoContext *ctx,
                                   const ImuGyroAccelData *samples,
                                   uint16_t count);
const char *gesture_result_name(GestureResult result);

#ifdef __cplusplus
}
#endif

#endif
