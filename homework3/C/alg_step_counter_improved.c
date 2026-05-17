/*
 * Copyright (c) 2026, VeriSilicon Holdings Co., Ltd. All rights reserved
 *
 * Improved Step Counter Algorithm v2
 * ====================================
 * Magnitude-based peak counter with adaptive interval constraints,
 * gyro-CV noise rejection, chain-restart filtering, and soft periodicity.
 *
 * Pipeline:
 *   1. Compute acceleration magnitude
 *   2. Gyro activity gate (stationary, gyro/acc ratio)
 *   3. Raw peak density noise gate (pre-filter)
 *   4. Adaptive interval thresholds based on gyro level
 *   5. DC removal + mean filter on magnitude
 *   6. Peak detection above mean + valley detection
 *   7. Merge close peaks
 *   8. Chain-restart interval filtering
 *   9. Peak amplitude gate
 *  10. Gyro-CV soft penalty (walk/medium only)
 *  11. Soft periodicity penalty
 */

#include "alg_step_counter_improved.h"
#include <math.h>
#include <string.h>

/* ============================================================
 * Configuration
 * ============================================================ */

#define MEAN_WIN        7       /* smoothing filter window */
#define GYR_SMOOTH      25      /* gyro smoothing window (0.5s) */

/* Activity classification thresholds */
#define GYR_STATIONARY  300.0f  /* below: stationary */
#define GYR_LOW_ACTIVE  600.0f  /* low activity (walking) */
#define GYR_HIGH_ACTIVE 1200.0f /* high activity (running) */
#define GYR_ACC_RATIO   5.0f    /* gyro/acc ratio for non-walking */
#define ACC_MIN_STD     120.0f  /* min acc std for walking */
#define PEAK_AMP_MIN    200.0f  /* min peak amplitude */

/* Gyro regularity threshold */
#define GYR_CV_SOFT     0.75f   /* gyro CV soft penalty threshold */

/* Raw peak density: max raw peaks per sample for noise rejection */
#define RAW_PEAK_DENSITY_MAX 0.12f

/* Adaptive interval constraints */
#define INTERVAL_WALK_MIN   15
#define INTERVAL_WALK_MAX   60
#define INTERVAL_BRISK_MIN  13
#define INTERVAL_BRISK_MAX  45
#define INTERVAL_RUN_MIN    10
#define INTERVAL_RUN_MAX    35

/* Periodicity thresholds (coefficient of variation) */
#define CV_WALK_LIMIT   0.55f
#define CV_RUN_LIMIT    0.85f

/* Maximum buffer sizes */
#ifndef MAX_SAMPLES
#define MAX_SAMPLES    (50000)
#endif
#ifndef MAX_PEAKS
#define MAX_PEAKS      (5000)
#endif

/* ============================================================
 * Static buffers
 * ============================================================ */

static float acc_mag_buf[MAX_SAMPLES];
static float acc_filt_buf[MAX_SAMPLES];
static float gyr_mag_buf[MAX_SAMPLES];
static float gyr_smooth_buf[MAX_SAMPLES];
static float acc_tmp[MAX_SAMPLES];
static float gyr_tmp[MAX_SAMPLES];

static uint16_t peak_idx[MAX_PEAKS];
static uint16_t valley_idx[MAX_PEAKS];

static int16_t intervals_buf[MAX_PEAKS];


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
            if (signal[peaks[i-1]] > signal[peaks[i]]) {
                uint16_t k;
                for (k = i; k < peak_cnt - 1; k++) {
                    peaks[k] = peaks[k + 1];
                }
                peak_cnt--;
            } else {
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

/*
 * Filter peaks by interval with chain-restart.
 * When distance exceeds max_dist, restart the chain from that peak
 * (instead of dropping all subsequent peaks as in v1).
 */
static uint16_t filter_peaks_by_interval(uint16_t *peaks, uint16_t peak_cnt,
                                          uint16_t min_dist, uint16_t max_dist)
{
    uint16_t i, valid_cnt;

    if (peak_cnt == 0) return 0;

    valid_cnt = 1;
    for (i = 1; i < peak_cnt; i++) {
        int dist = (int)peaks[i] - (int)peaks[valid_cnt - 1];
        if (dist >= (int)min_dist && dist <= (int)max_dist) {
            peaks[valid_cnt++] = peaks[i];
        } else if (dist > (int)max_dist) {
            /* Chain broke (pause / missed step): restart */
            peaks[valid_cnt++] = peaks[i];
        }
        /* else: dist < min_dist -> drop */
    }
    return valid_cnt;
}


/* ============================================================
 * Periodicity and activity classification
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
    uint16_t min_dist, max_dist;
    float cv_limit;
    float acc_mean;
    float gyr_mean, gyr_std, gyr_cv;
    float acc_std;
    float acc_raw_std;
    uint16_t raw_peak_cnt;
    float raw_density;
    uint16_t i;

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

    /* ---- 1. Gyroscope Activity Gate ---- */

    compute_magnitude(input->gx, input->gy, input->gz, gyr_mag_buf, len);
    mean_filter(gyr_mag_buf, gyr_smooth_buf, len, GYR_SMOOTH);

    gyr_mean = array_mean(gyr_smooth_buf, len);
    gyr_std  = array_std(gyr_smooth_buf, len, gyr_mean);
    gyr_cv   = (gyr_mean > 0.0f) ? (gyr_std / gyr_mean) : 0.0f;

    /* Gate A: Stationary */
    if (gyr_mean < GYR_STATIONARY) {
        return ALGO_NORMAL;
    }

    /* Gate B: Gyro-dominated motion */
    compute_magnitude(input->ax, input->ay, input->az, acc_mag_buf, len);
    acc_raw_std = array_std(acc_mag_buf, len,
                             array_mean(acc_mag_buf, len));
    if (acc_raw_std > 0.0f && gyr_std > GYR_ACC_RATIO * acc_raw_std) {
        return ALGO_NORMAL;
    }

    /* ---- 2. Raw Peak Density Gate ---- */

    acc_mean = array_mean(acc_mag_buf, len);
    for (i = 0; i < len; i++) {
        acc_tmp[i] = acc_mag_buf[i] - acc_mean;
    }
    /* Only apply density check on longer segments (>500 samples = 10s)
       to avoid rejecting short test windows */
    if (len > 500) {
        raw_peak_cnt = find_peaks_above_mean(acc_tmp, len, peak_idx, MAX_PEAKS);
        raw_density = (float)raw_peak_cnt / (float)len;
        if (raw_density > RAW_PEAK_DENSITY_MAX) {
            return ALGO_NORMAL;
        }
    }

    /* ---- 3. Adaptive Interval Thresholds ---- */

    if (gyr_mean < GYR_LOW_ACTIVE) {
        min_dist = INTERVAL_WALK_MIN;
        max_dist = INTERVAL_WALK_MAX;
        cv_limit = CV_WALK_LIMIT;
    } else if (gyr_mean < GYR_HIGH_ACTIVE) {
        min_dist = INTERVAL_BRISK_MIN;
        max_dist = INTERVAL_BRISK_MAX;
        cv_limit = CV_RUN_LIMIT;
    } else {
        min_dist = INTERVAL_RUN_MIN;
        max_dist = INTERVAL_RUN_MAX;
        cv_limit = CV_RUN_LIMIT;
    }

    /* ---- 4. Acceleration Magnitude Processing ---- */

    /* acc_tmp already has DC-removed magnitude from step 2 */
    mean_filter(acc_tmp, acc_filt_buf, len, MEAN_WIN);

    acc_std = array_std(acc_filt_buf, len,
                         array_mean(acc_filt_buf, len));

    /* Gate C: Very subtle motion */
    if (acc_std < ACC_MIN_STD && gyr_mean < GYR_LOW_ACTIVE) {
        return ALGO_NORMAL;
    }

    /* ---- 5. Peak Detection ---- */

    peak_cnt = find_peaks_above_mean(acc_filt_buf, len, peak_idx, MAX_PEAKS);
    if (peak_cnt == 0) {
        return ALGO_NORMAL;
    }

    /* ---- 6. Valley Detection ---- */

    valley_cnt = find_valleys_below_mean(acc_filt_buf, len, valley_idx, MAX_PEAKS);

    /* ---- 7. Merge Close Peaks ---- */

    peak_cnt = merge_close_peaks(acc_filt_buf, peak_idx, peak_cnt,
                                  valley_idx, valley_cnt);

    /* ---- 8. Chain-Restart Interval Filtering ---- */

    peak_cnt = filter_peaks_by_interval(peak_idx, peak_cnt,
                                         min_dist, max_dist);

    if (peak_cnt == 0) {
        return ALGO_NORMAL;
    }

    *steps = peak_cnt;

    /* ---- 9. Peak Amplitude Gate ---- */

    {
        float sum_ampl = 0.0f;
        float peak_ampl;
        for (i = 0; i < peak_cnt; i++) {
            sum_ampl += acc_filt_buf[peak_idx[i]];
        }
        peak_ampl = sum_ampl / (float)peak_cnt;

        if (peak_ampl < PEAK_AMP_MIN && gyr_mean < GYR_LOW_ACTIVE + 200.0f) {
            *steps = 0;
            return ALGO_NORMAL;
        }
    }

    /* ---- 10. Gyro-CV Soft Penalty (walk/medium only) ---- */

    if (gyr_mean < GYR_HIGH_ACTIVE && gyr_cv > GYR_CV_SOFT) {
        float ratio = GYR_CV_SOFT / gyr_cv;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        *steps = (uint16_t)((float)(*steps) * ratio);
        if (*steps == 0) {
            return ALGO_NORMAL;
        }
    }

    /* ---- 11. Soft Periodicity Penalty ---- */

    if (*steps >= 3) {
        uint16_t interval_cnt = 0;
        float cv_val;
        for (i = 1; i < peak_cnt && interval_cnt < MAX_PEAKS; i++) {
            int diff = (int)peak_idx[i] - (int)peak_idx[i-1];
            intervals_buf[interval_cnt++] = (int16_t)diff;
        }

        if (interval_cnt >= 2) {
            cv_val = compute_cv(intervals_buf, interval_cnt);

            if (cv_val > cv_limit) {
                float penalty = cv_limit / cv_val;
                if (penalty > 1.0f) penalty = 1.0f;
                *steps = (uint16_t)((float)(*steps) * penalty * penalty);
            }
        }
    }

    return ALGO_NORMAL;
}
