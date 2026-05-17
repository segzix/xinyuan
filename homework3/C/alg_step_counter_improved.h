/*
 * Copyright (c) 2026, VeriSilicon Holdings Co., Ltd. All rights reserved
 */

#ifndef __ALG_STEP_COUNTER_IMPROVED_H__
#define __ALG_STEP_COUNTER_IMPROVED_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Sampling rate */
#define IMP_ACC_FS (50)

/** @brief Gravity constant in raw units */
#define IMP_ACC_GRAVITY (4096)

/**
 * @brief 6-axis IMU input data structure
 */
typedef struct ImuInput {
    uint16_t len;    /**< number of samples */
    int16_t *ax;     /**< accelerometer X */
    int16_t *ay;     /**< accelerometer Y */
    int16_t *az;     /**< accelerometer Z */
    int16_t *gx;     /**< gyroscope X */
    int16_t *gy;     /**< gyroscope Y */
    int16_t *gz;     /**< gyroscope Z */
} ImuInput;

/**
 * @brief error codes
 */
typedef enum AlgoError {
    ALGO_NORMAL      = 0,
    ALGO_ERR_GENERIC = 1,
} AlgoError;

/**
 * @brief Initialize the improved step counter.
 * Must be called once before step_counter_process().
 */
AlgoError step_counter_improved_init(void);

/**
 * @brief Process a complete IMU data segment and count steps.
 *
 * This function processes the entire input data in one call
 * (batch mode). For real-time use, call this with sliding
 * windows of data.
 *
 * @param input     IMU input data (acc + gyro)
 * @param steps     output: detected step count
 * @return          ALGO_NORMAL on success
 */
AlgoError step_counter_improved_process(ImuInput *input, uint16_t *steps);

#ifdef __cplusplus
}
#endif

#endif /* __ALG_STEP_COUNTER_IMPROVED_H__ */
