#include "gesture_algo.h"

#include <string.h>

#define WINDOW_SAMPLES 100u
#define MIN_CLASSIFY_SAMPLES 50u
#define COOLDOWN_SAMPLES 100u
#define EVAL_INTERVAL_SAMPLES 25u

typedef struct WindowFeatures {
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
    uint32_t accel_jerk_sum;
    uint32_t gyro_energy_sum;
    uint32_t peak_accel;
    uint32_t peak_gyro;
    uint32_t accel_range;
    uint32_t gyro_range;
    int32_t z_delta;
    int32_t y_delta;
    int32_t x_delta;
    int32_t mean_az;
    int32_t mean_ay;
} WindowFeatures;

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

static inline void update_min_max16(int16_t value, int16_t *min_value, int16_t *max_value)
{
    if (value < *min_value) {
        *min_value = value;
    }
    if (value > *max_value) {
        *max_value = value;
    }
}

static void init_window_features(WindowFeatures *features, const ImuGyroAccelData *first_sample)
{
    memset(features, 0, sizeof(*features));
    features->min_ax = features->max_ax = first_sample->ax;
    features->min_ay = features->max_ay = first_sample->ay;
    features->min_az = features->max_az = first_sample->az;
    features->min_gx = features->max_gx = first_sample->gx;
    features->min_gy = features->max_gy = first_sample->gy;
    features->min_gz = features->max_gz = first_sample->gz;
}

static void update_window_features(WindowFeatures *features,
                                   const ImuGyroAccelData *samples,
                                   uint16_t index)
{
    uint32_t accel_abs;
    uint32_t gyro_abs;

    update_min_max16(samples[index].ax, &features->min_ax, &features->max_ax);
    update_min_max16(samples[index].ay, &features->min_ay, &features->max_ay);
    update_min_max16(samples[index].az, &features->min_az, &features->max_az);
    update_min_max16(samples[index].gx, &features->min_gx, &features->max_gx);
    update_min_max16(samples[index].gy, &features->min_gy, &features->max_gy);
    update_min_max16(samples[index].gz, &features->min_gz, &features->max_gz);

    if (index > 0u) {
        features->accel_jerk_sum +=
            (uint32_t)iabs32((int32_t)samples[index].ax - samples[index - 1u].ax);
        features->accel_jerk_sum +=
            (uint32_t)iabs32((int32_t)samples[index].ay - samples[index - 1u].ay);
        features->accel_jerk_sum +=
            (uint32_t)iabs32((int32_t)samples[index].az - samples[index - 1u].az);
    }

    accel_abs = iabs32(samples[index].ax) + iabs32(samples[index].ay)
                + iabs32(samples[index].az);
    gyro_abs = iabs32(samples[index].gx) + iabs32(samples[index].gy)
               + iabs32(samples[index].gz);
    if (accel_abs > features->peak_accel) {
        features->peak_accel = accel_abs;
    }
    if (gyro_abs > features->peak_gyro) {
        features->peak_gyro = gyro_abs;
    }
    features->gyro_energy_sum += gyro_abs;
    features->mean_az += samples[index].az;
    features->mean_ay += samples[index].ay;
}

static void finalize_window_features(WindowFeatures *features,
                                     const ImuGyroAccelData *samples,
                                     uint16_t count)
{
    features->accel_range = range16(features->min_ax, features->max_ax)
                            + range16(features->min_ay, features->max_ay)
                            + range16(features->min_az, features->max_az);
    features->gyro_range = range16(features->min_gx, features->max_gx)
                           + range16(features->min_gy, features->max_gy)
                           + range16(features->min_gz, features->max_gz);
    features->z_delta = (int32_t)samples[count - 1u].az - samples[0].az;
    features->y_delta = (int32_t)samples[count - 1u].ay - samples[0].ay;
    features->x_delta = (int32_t)samples[count - 1u].ax - samples[0].ax;
    features->mean_az /= (int32_t)count;
    features->mean_ay /= (int32_t)count;
}

static void compute_window_features(const ImuGyroAccelData *samples,
                                    uint16_t count,
                                    WindowFeatures *features)
{
    uint16_t i;

    init_window_features(features, &samples[0]);

    /* WINDOW_SAMPLES=100 keeps worst-case sums below uint32_t range. */
    for (i = 0u; i < count; i++) {
        update_window_features(features, samples, i);
    }

    finalize_window_features(features, samples, count);
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

static void append_window_sample(GestureAlgoContext *ctx, const ImuGyroAccelData *sample)
{
    if (ctx->window_count < WINDOW_SAMPLES) {
        ctx->window[ctx->window_count++] = *sample;
        return;
    }

    memmove(&ctx->window[0], &ctx->window[1], (WINDOW_SAMPLES - 1u) * sizeof(ctx->window[0]));
    ctx->window[WINDOW_SAMPLES - 1u] = *sample;
}

static void tick_classify_timers(GestureAlgoContext *ctx)
{
    if (ctx->cooldown_samples > 0u) {
        ctx->cooldown_samples--;
    }

    if (ctx->eval_countdown > 0u) {
        ctx->eval_countdown--;
    }
}

static GestureResult classify_window(const ImuGyroAccelData *samples, uint16_t count)
{
    WindowFeatures features;

    if (count < MIN_CLASSIFY_SAMPLES) {
        return GESTURE_OTHER;
    }

    compute_window_features(samples, count, &features);

    /* up/down are whole-wrist rotations: high gyro energy and coherent y/z movement. */
    if (features.gyro_energy_sum > 100000u && features.gyro_energy_sum < 280000u
        && features.peak_gyro > 5000u && features.peak_gyro < 18000u
        && features.gyro_range > 7000u && range16(features.min_ay, features.max_ay) > 5500u
        && features.peak_accel < 18000u && iabs32(features.x_delta) < 4500u
        && features.y_delta > 2800 && features.z_delta > 500) {
        return GESTURE_UP;
    }
    if (features.gyro_energy_sum > 100000u && features.gyro_energy_sum < 280000u
        && features.peak_gyro > 5000u && features.peak_gyro < 18000u
        && features.gyro_range > 7000u && range16(features.min_ay, features.max_ay) > 5500u
        && features.peak_accel < 18000u && iabs32(features.x_delta) < 4500u
        && features.y_delta < -2800 && features.z_delta < -500) {
        return GESTURE_DOWN;
    }

    /* pinch/clench are detected only in the wrist-neutral posture seen in the dataset. */
    if (features.mean_az < 2500 || features.mean_ay < -1800
        || features.gyro_energy_sum > 60000u || iabs32(features.y_delta) > 1000u
        || iabs32(features.z_delta) > 1000u) {
        return GESTURE_OTHER;
    }

    if (features.accel_range > 7000u && features.accel_range < 22000u
        && features.gyro_range > 2800u && features.gyro_range < 9000u
        && features.accel_jerk_sum > 35000u && features.peak_gyro > 1300u
        && features.peak_gyro < 7000u && features.peak_accel > 8500u
        && features.peak_accel * 2u > features.peak_gyro * 3u) {
        return GESTURE_CLENCH;
    }
    if (features.accel_range > 1800u && features.accel_range < 5500u
        && features.gyro_range > 900u && features.gyro_range < 3000u
        && features.accel_jerk_sum > 9000u && features.accel_jerk_sum < 30000u
        && features.gyro_energy_sum < 18000u
        && features.peak_gyro > 450u && features.peak_gyro < 1600u
        && features.peak_accel < 8500u) {
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
        append_window_sample(ctx, &samples[i]);
        tick_classify_timers(ctx);

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
