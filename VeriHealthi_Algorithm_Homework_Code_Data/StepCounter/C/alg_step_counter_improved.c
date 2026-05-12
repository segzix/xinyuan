/*
 * Copyright (c) 2026, VeriSilicon Holdings Co., Ltd. All rights reserved
 *
 * Improved Step Counter Algorithm
 * ================================
 * Adaptive interval-constrained peak counter with gyroscope-based
 * activity classification.
 *
 * Pipeline:
 *   1. Compute acceleration magnitude sqrt(ax^2 + ay^2 + az^2)
 *   2. Remove DC component and apply mean filter
 *   3. Detect peaks above mean
 *   4. Merge close peaks (no intervening valley)
 *   5. Apply interval constraints (min/max distance)
 *   6. Gyroscope-based activity classification
 *   7. Periodicity verification
 */

#include "alg_step_counter_improved.h"
#include <math.h>
#include <string.h>

/* ============================================================
 * Configuration
 * ============================================================ */

#define MEAN_WIN        7       /* smoothing filter window */
#define GYR_SMOOTH      25      /* gyro smoothing window (0.5s) */
#define MIN_PK_DIST     15      /* min peak distance (0.3s) */
#define MAX_PK_DIST     100     /* max peak distance (2s) */

/* Activity classification thresholds */
#define GYR_STATIONARY  300.0f  /* below: stationary */
#define GYR_LOW_ACTIVE  600.0f  /* low activity */
#define GYR_HIGH_ACTIVE 1200.0f /* high activity */
#define GYR_ACC_RATIO   3.0f    /* gyro/acc ratio for non-walking */
#define ACC_MIN_STD     150.0f  /* min acc std for walking */
#define PEAK_AMP_MIN    200.0f  /* min peak amplitude */

/* Periodicity thresholds (coefficient of variation) */
#define CV_LOW_GYR      0.5f    /* strict regularity */
#define CV_MED_GYR      0.7f    /* medium regularity */
#define CV_HIGH_GYR     0.8f    /* relaxed regularity */

/* Maximum buffer sizes (adjust based on available memory) */
#define MAX_SAMPLES    (50000)  /* max input samples */
#define MAX_PEAKS      (5000)   /* max detected peaks */

/* ============================================================
 * Static buffers
 * ============================================================ */

static float acc_mag_buf[MAX_SAMPLES];   /* acceleration magnitude */
static float acc_filt_buf[MAX_SAMPLES];  /* filtered acceleration */
static float gyr_mag_buf[MAX_SAMPLES];   /* gyroscope magnitude */
static float gyr_smooth_buf[MAX_SAMPLES]; /* smoothed gyroscope */
static float acc_tmp[MAX_SAMPLES];       /* temporary workspace */
static float gyr_tmp[MAX_SAMPLES];       /* temporary workspace */

static uint16_t peak_idx[MAX_PEAKS];      /* peak indices */
static uint16_t valley_idx[MAX_PEAKS];    /* valley indices */

/* Note: intervals_buf is file-scope static to avoid stack overflow on
   embedded MCUs where the stack may be only 4-8 KB. */
static int16_t intervals_buf[MAX_PEAKS];  /* peak interval buffer for CV */

/* ============================================================
 * Math utilities
 * ============================================================ */

static float array_mean(const float *a, uint16_t len)
{
    float sum = 0.0f;
    uint16_t i;
    if (len == 0) return 0.0f;
    for (i = 0; i < len; i++) {
        sum += a[i];
    }
    return sum / (float)len;
}

static float array_std(const float *a, uint16_t len, float mean_val)
{
    float sum_sq = 0.0f;
    uint16_t i;
    if (len <= 1) return 0.0f;
    for (i = 0; i < len; i++) {
        float diff = a[i] - mean_val;
        sum_sq += diff * diff;
    }
    return sqrtf(sum_sq / (float)len);
}

/* ============================================================
 * Signal processing
 * ============================================================ */

static void mean_filter(const float *input, float *output, uint16_t len,
                         uint16_t window)
{
    uint16_t i, j;
    uint16_t half = window / 2;
    uint16_t start, end;
    float sum;
    uint16_t count;

    for (i = 0; i < len; i++) {
        start = (i > half) ? (uint16_t)(i - half) : 0;
        end   = (i + half < len) ? (uint16_t)(i + half) : (uint16_t)(len - 1);
        sum   = 0.0f;
        count = 0;
        for (j = start; j <= end; j++) {
            sum += input[j];
            count++;
        }
        output[i] = sum / (float)count;
    }
}

static void compute_magnitude(const int16_t *x, const int16_t *y,
                               const int16_t *z, float *mag, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        float fx = (float)x[i];
        float fy = (float)y[i];
        float fz = (float)z[i];
        mag[i] = sqrtf(fx * fx + fy * fy + fz * fz);
    }
}

/* ============================================================
 * Peak / valley detection
 * ============================================================ */

static uint16_t find_peaks_above_mean(const float *signal, uint16_t len,
                                       uint16_t *peaks, uint16_t max_peaks)
{
    float m;
    uint16_t cnt = 0;
    uint16_t i;

    if (len < 2) return 0;

    m = array_mean(signal, len);

    for (i = 1; i < len - 1 && cnt < max_peaks; i++) {
        if (signal[i] >= signal[i-1] && signal[i] > signal[i+1]
            && signal[i] > m) {
            peaks[cnt++] = i;
        }
    }
    return cnt;
}

static uint16_t find_valleys_below_mean(const float *signal, uint16_t len,
                                         uint16_t *valleys, uint16_t max_valleys)
{
    float m;
    uint16_t cnt = 0;
    uint16_t i;

    if (len < 2) return 0;

    m = array_mean(signal, len);

    for (i = 1; i < len - 1 && cnt < max_valleys; i++) {
        if (signal[i] <= signal[i-1] && signal[i] < signal[i+1]
            && signal[i] < m) {
            valleys[cnt++] = i;
        }
    }
    return cnt;
}

static uint16_t merge_close_peaks(const float *signal,
                                   uint16_t *peaks, uint16_t peak_cnt,
                                   const uint16_t *valleys, uint16_t valley_cnt)
{
    uint16_t i, j;
    int found;

    if (peak_cnt <= 1) return peak_cnt;

    i = 1;
    while (i < peak_cnt) {
        found = 0;
        for (j = 0; j < valley_cnt; j++) {
            if (valleys[j] > peaks[i-1] && valleys[j] < peaks[i]) {
                found = 1;
                break;
            }
        }
        if (!found) {
            /* No valley between adjacent peaks: keep the higher one */
            if (signal[peaks[i-1]] > signal[peaks[i]]) {
                /* Remove peaks[i] by shifting left */
                uint16_t k;
                for (k = i; k < peak_cnt - 1; k++) {
                    peaks[k] = peaks[k + 1];
                }
                peak_cnt--;
            } else {
                /* Remove peaks[i-1] by shifting left */
                uint16_t k;
                for (k = i - 1; k < peak_cnt - 1; k++) {
                    peaks[k] = peaks[k + 1];
                }
                peak_cnt--;
            }
        } else {
            i++;
        }
    }
    return peak_cnt;
}

static uint16_t filter_peaks_by_interval(uint16_t *peaks, uint16_t peak_cnt,
                                          uint16_t min_dist, uint16_t max_dist)
{
    uint16_t i, valid_cnt;

    if (peak_cnt == 0) return 0;

    valid_cnt = 1; /* keep first peak */
    for (i = 1; i < peak_cnt; i++) {
        int dist = (int)peaks[i] - (int)peaks[valid_cnt - 1];
        if (dist >= (int)min_dist && dist <= (int)max_dist) {
            peaks[valid_cnt++] = peaks[i];
        }
    }
    return valid_cnt;
}

/* ============================================================
 * Activity classification
 * ============================================================ */

static float compute_cv(const int16_t *values, uint16_t count)
{
    float mean_val, std_val;
    float sum = 0.0f, sum_sq = 0.0f;
    uint16_t i;

    if (count < 2) return 0.0f;

    for (i = 0; i < count; i++) {
        sum += (float)values[i];
    }
    mean_val = sum / (float)count;

    for (i = 0; i < count; i++) {
        float diff = (float)values[i] - mean_val;
        sum_sq += diff * diff;
    }
    std_val = sqrtf(sum_sq / (float)count);

    if (mean_val < 1.0f) return 0.0f;
    return std_val / mean_val;
}

static uint16_t classify_and_count(const float *acc_filt, uint16_t acc_len,
                                     const float *gyr_smooth, uint16_t gyr_len,
                                     uint16_t *peaks, uint16_t peak_cnt)
{
    float gyr_mean, gyr_std, acc_std, peak_ampl, cv_val;
    float sum_ampl;
    uint16_t i;
    uint16_t interval_cnt;

    if (peak_cnt == 0) return 0;
    if (gyr_len == 0 || acc_len == 0) return 0;

    /* Gyroscope statistics */
    gyr_mean = array_mean(gyr_smooth, gyr_len);
    gyr_std  = array_std(gyr_smooth, gyr_len, gyr_mean);

    /* Acceleration statistics */
    acc_std = array_std(acc_filt, acc_len,
                         array_mean(acc_filt, acc_len));

    /* Gate A: stationary */
    if (gyr_mean < GYR_STATIONARY) {
        return 0;
    }

    /* Gate B: gyro-dominated motion */
    if (acc_std > 0.0f && gyr_std > GYR_ACC_RATIO * acc_std) {
        return 0;
    }

    /* Gate C: subtle motion */
    if (acc_std < ACC_MIN_STD && gyr_mean < GYR_LOW_ACTIVE) {
        return 0;
    }

    /* Gate D: peak amplitude */
    sum_ampl = 0.0f;
    for (i = 0; i < peak_cnt; i++) {
        sum_ampl += acc_filt[peaks[i]];
    }
    peak_ampl = sum_ampl / (float)peak_cnt;

    if (peak_ampl < PEAK_AMP_MIN && gyr_mean < GYR_LOW_ACTIVE + 200.0f) {
        return 0;
    }

    /* Periodicity: compute CV of peak intervals */
    interval_cnt = 0;
    for (i = 1; i < peak_cnt && interval_cnt < MAX_PEAKS; i++) {
        int diff = (int)peaks[i] - (int)peaks[i-1];
        intervals_buf[interval_cnt++] = (int16_t)diff;
    }

    if (interval_cnt < 2) {
        cv_val = 0.0f;
    } else {
        cv_val = compute_cv(intervals_buf, interval_cnt);
    }

    /* Activity classification */
    if (gyr_mean < GYR_LOW_ACTIVE) {
        if (cv_val < CV_LOW_GYR && peak_cnt >= 2)
            return peak_cnt;
        return 0;
    } else if (gyr_mean < GYR_HIGH_ACTIVE) {
        if (cv_val < CV_MED_GYR && peak_cnt >= 2)
            return peak_cnt;
        return 0;
    } else {
        if (cv_val < CV_HIGH_GYR && peak_cnt >= 2)
            return peak_cnt;
        return 0; /* high gyro but single peak or irregular → reject */
    }
}

/* ============================================================
 * Public interface
 * ============================================================ */

AlgoError step_counter_improved_init(void)
{
    memset(acc_mag_buf, 0, sizeof(acc_mag_buf));
    memset(acc_filt_buf, 0, sizeof(acc_filt_buf));
    memset(gyr_mag_buf, 0, sizeof(gyr_mag_buf));
    memset(gyr_smooth_buf, 0, sizeof(gyr_smooth_buf));
    memset(acc_tmp, 0, sizeof(acc_tmp));
    memset(gyr_tmp, 0, sizeof(gyr_tmp));
    memset(peak_idx, 0, sizeof(peak_idx));
    memset(valley_idx, 0, sizeof(valley_idx));
    return ALGO_NORMAL;
}

AlgoError step_counter_improved_process(ImuInput *input, uint16_t *steps)
{
    uint16_t len, peak_cnt, valley_cnt;
    float acc_mean;

    if (!input || !steps || input->len == 0) {
        return ALGO_ERR_GENERIC;
    }
    if (!input->ax || !input->ay || !input->az ||
        !input->gx || !input->gy || !input->gz) {
        return ALGO_ERR_GENERIC;
    }
    if (input->len > MAX_SAMPLES) {
        return ALGO_ERR_GENERIC;
    }

    len = input->len;
    *steps = 0;

    /* Step 1: Compute acceleration magnitude */
    compute_magnitude(input->ax, input->ay, input->az, acc_mag_buf, len);

    /* Step 2: Remove DC and apply mean filter */
    acc_mean = array_mean(acc_mag_buf, len);
    {
        uint16_t i;
        for (i = 0; i < len; i++) {
            acc_tmp[i] = acc_mag_buf[i] - acc_mean;
        }
    }
    mean_filter(acc_tmp, acc_filt_buf, len, MEAN_WIN);

    /* Step 3: Compute gyroscope magnitude */
    compute_magnitude(input->gx, input->gy, input->gz, gyr_mag_buf, len);
    mean_filter(gyr_mag_buf, gyr_smooth_buf, len, GYR_SMOOTH);

    /* Step 4: Peak detection */
    peak_cnt = find_peaks_above_mean(acc_filt_buf, len, peak_idx, MAX_PEAKS);
    if (peak_cnt == 0) {
        return ALGO_NORMAL;
    }

    /* Step 5: Valley detection */
    valley_cnt = find_valleys_below_mean(acc_filt_buf, len, valley_idx, MAX_PEAKS);

    /* Step 6: Merge close peaks */
    peak_cnt = merge_close_peaks(acc_filt_buf, peak_idx, peak_cnt,
                                  valley_idx, valley_cnt);

    /* Step 7: Interval-constrained filtering */
    peak_cnt = filter_peaks_by_interval(peak_idx, peak_cnt,
                                         MIN_PK_DIST, MAX_PK_DIST);

    /* Step 8: Activity classification and final count */
    *steps = classify_and_count(acc_filt_buf, len, gyr_smooth_buf, len,
                                 peak_idx, peak_cnt);

    return ALGO_NORMAL;
}
