#include "gesture_algo.h"

#include <string.h>

#define WINDOW_SAMPLES 100u
#define MIN_CLASSIFY_SAMPLES 50u
#define COOLDOWN_SAMPLES 100u
#define EVAL_INTERVAL_SAMPLES 25u

static uint32_t iabs32(int32_t value)
{
    if (value >= 0) {
        return (uint32_t)value;
    }
    return (uint32_t)(-(value + 1)) + 1u;
}

static uint32_t range16(int16_t min_value, int16_t max_value)
{
    return (uint32_t)((int32_t)max_value - (int32_t)min_value);
}

const char *gesture_result_name(GestureResult result)
{
    switch (result) {
    case GESTURE_PINCH:
        return "pinch";
    case GESTURE_CLENCH:
        return "clench";
    case GESTURE_UP:
        return "up";
    case GESTURE_DOWN:
        return "down";
    case GESTURE_OTHER:
    default:
        return "other";
    }
}

void gesture_algo_init(GestureAlgoContext *ctx)
{
    if (ctx != NULL) {
        memset(ctx, 0, sizeof(*ctx));
    }
}

static GestureResult classify_window(const ImuGyroAccelData *samples, uint16_t count)
{
    uint16_t i;
    int16_t min_ax;
    int16_t max_ax;
    int16_t min_ay;
    int16_t max_ay;
    int16_t min_az;
    int16_t max_az;
    int16_t min_gx;
    int16_t max_gx;
    int16_t min_gy;
    int16_t max_gy;
    int16_t min_gz;
    int16_t max_gz;
    uint32_t accel_jerk_sum = 0u;
    uint32_t gyro_energy_sum = 0u;
    uint32_t peak_accel = 0u;
    uint32_t peak_gyro = 0u;
    uint32_t accel_range;
    uint32_t gyro_range;
    int32_t z_delta;
    int32_t y_delta;
    int32_t x_delta;
    int32_t mean_az = 0;
    int32_t mean_ay = 0;

    if (count < MIN_CLASSIFY_SAMPLES) {
        return GESTURE_OTHER;
    }

    min_ax = max_ax = samples[0].ax;
    min_ay = max_ay = samples[0].ay;
    min_az = max_az = samples[0].az;
    min_gx = max_gx = samples[0].gx;
    min_gy = max_gy = samples[0].gy;
    min_gz = max_gz = samples[0].gz;

    /* WINDOW_SAMPLES=100 keeps worst-case sums below uint32_t range. */
    for (i = 0u; i < count; i++) {
        uint32_t accel_abs;
        uint32_t gyro_abs;

        if (samples[i].ax < min_ax) {
            min_ax = samples[i].ax;
        } else if (samples[i].ax > max_ax) {
            max_ax = samples[i].ax;
        }
        if (samples[i].ay < min_ay) {
            min_ay = samples[i].ay;
        } else if (samples[i].ay > max_ay) {
            max_ay = samples[i].ay;
        }
        if (samples[i].az < min_az) {
            min_az = samples[i].az;
        } else if (samples[i].az > max_az) {
            max_az = samples[i].az;
        }
        if (samples[i].gx < min_gx) {
            min_gx = samples[i].gx;
        } else if (samples[i].gx > max_gx) {
            max_gx = samples[i].gx;
        }
        if (samples[i].gy < min_gy) {
            min_gy = samples[i].gy;
        } else if (samples[i].gy > max_gy) {
            max_gy = samples[i].gy;
        }
        if (samples[i].gz < min_gz) {
            min_gz = samples[i].gz;
        } else if (samples[i].gz > max_gz) {
            max_gz = samples[i].gz;
        }

        accel_abs = iabs32(samples[i].ax) + iabs32(samples[i].ay) + iabs32(samples[i].az);
        gyro_abs = iabs32(samples[i].gx) + iabs32(samples[i].gy) + iabs32(samples[i].gz);
        if (accel_abs > peak_accel) {
            peak_accel = accel_abs;
        }
        if (gyro_abs > peak_gyro) {
            peak_gyro = gyro_abs;
        }
        gyro_energy_sum += gyro_abs;
        mean_az += samples[i].az;
        mean_ay += samples[i].ay;
    }

    for (i = 1u; i < count; i++) {
        accel_jerk_sum += (uint32_t)iabs32((int32_t)samples[i].ax - samples[i - 1u].ax);
        accel_jerk_sum += (uint32_t)iabs32((int32_t)samples[i].ay - samples[i - 1u].ay);
        accel_jerk_sum += (uint32_t)iabs32((int32_t)samples[i].az - samples[i - 1u].az);
    }

    accel_range = range16(min_ax, max_ax) + range16(min_ay, max_ay) + range16(min_az, max_az);
    gyro_range = range16(min_gx, max_gx) + range16(min_gy, max_gy) + range16(min_gz, max_gz);
    z_delta = (int32_t)samples[count - 1u].az - samples[0].az;
    y_delta = (int32_t)samples[count - 1u].ay - samples[0].ay;
    x_delta = (int32_t)samples[count - 1u].ax - samples[0].ax;
    mean_az /= (int32_t)count;
    mean_ay /= (int32_t)count;

    /* up/down are whole-wrist rotations: high gyro energy and coherent y/z movement. */
    if (gyro_energy_sum > 100000u && gyro_energy_sum < 280000u
        && peak_gyro > 5000u && peak_gyro < 18000u
        && gyro_range > 7000u && range16(min_ay, max_ay) > 5500u
        && peak_accel < 18000u && iabs32(x_delta) < 4500u
        && y_delta > 2800 && z_delta > 500) {
        return GESTURE_UP;
    }
    if (gyro_energy_sum > 100000u && gyro_energy_sum < 280000u
        && peak_gyro > 5000u && peak_gyro < 18000u
        && gyro_range > 7000u && range16(min_ay, max_ay) > 5500u
        && peak_accel < 18000u && iabs32(x_delta) < 4500u
        && y_delta < -2800 && z_delta < -500) {
        return GESTURE_DOWN;
    }

    /* pinch/clench are detected only in the wrist-neutral posture seen in the dataset. */
    if (mean_az < 2500 || mean_ay < -1800 || gyro_energy_sum > 60000u
        || iabs32(y_delta) > 1000u || iabs32(z_delta) > 1000u) {
        return GESTURE_OTHER;
    }

    if (accel_range > 6500u && accel_range < 22000u
        && gyro_range > 2800u && gyro_range < 9000u
        && accel_jerk_sum > 32000u && peak_gyro > 1300u && peak_accel > 7800u) {
        return GESTURE_CLENCH;
    }
    if (accel_range > 1800u && accel_range < 5500u
        && gyro_range > 900u && gyro_range < 3000u
        && accel_jerk_sum > 9000u && accel_jerk_sum < 30000u
        && peak_gyro > 450u && peak_gyro < 1600u && peak_accel < 8500u) {
        return GESTURE_PINCH;
    }

    return GESTURE_OTHER;
}

GestureResult gesture_algo_process(GestureAlgoContext *ctx,
                                   const ImuGyroAccelData *samples,
                                   uint16_t count)
{
    uint16_t i;
    GestureResult result = GESTURE_OTHER;

    if (ctx == NULL || samples == NULL || count == 0u) {
        return GESTURE_OTHER;
    }

    for (i = 0u; i < count; i++) {
        if (ctx->window_count < WINDOW_SAMPLES) {
            ctx->window[ctx->window_count++] = samples[i];
        } else {
            memmove(&ctx->window[0], &ctx->window[1], (WINDOW_SAMPLES - 1u) * sizeof(ctx->window[0]));
            ctx->window[WINDOW_SAMPLES - 1u] = samples[i];
        }

        if (ctx->cooldown_samples > 0u) {
            ctx->cooldown_samples--;
        }

        if (ctx->eval_countdown > 0u) {
            ctx->eval_countdown--;
        }
        if (result == GESTURE_OTHER && ctx->window_count >= MIN_CLASSIFY_SAMPLES
            && ctx->cooldown_samples == 0u && ctx->eval_countdown == 0u) {
            result = classify_window(ctx->window, ctx->window_count);
            ctx->eval_countdown = EVAL_INTERVAL_SAMPLES;
            if (result != GESTURE_OTHER) {
                ctx->cooldown_samples = COOLDOWN_SAMPLES;
            }
        }
    }

    return result;
}
