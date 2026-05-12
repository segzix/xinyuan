/*
 * Copyright (c) 2026, VeriSilicon Holdings Co., Ltd. All rights reserved
 *
 * Test harness for improved step counter (C version).
 */

#include "alg_step_counter_improved.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int16_t ax_buf[50000], ay_buf[50000], az_buf[50000];
static int16_t gx_buf[50000], gy_buf[50000], gz_buf[50000];

static void trim_newline(char *str)
{
    int len = (int)strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        len--;
    }
}

static int extract_truth(const char *filename)
{
    const char *p = strstr(filename, "_step");
    if (!p) return 0;
    p += 5;
    return atoi(p);
}

static int read_imu_file(const char *filepath,
                          int16_t *ax, int16_t *ay, int16_t *az,
                          int16_t *gx, int16_t *gy, int16_t *gz,
                          uint16_t *out_len)
{
    FILE *fd;
    char line[256];
    int found_type = 0;
    int cnt = 0;
    int16_t val;

    fd = fopen(filepath, "r");
    if (!fd) return -1;

    while (fgets(line, sizeof(line), fd)) {
        trim_newline(line);
        if (strstr(line, "TYPE") != NULL) {
            found_type = 1;
            break;
        }
    }

    if (!found_type) { fclose(fd); return -1; }

    cnt = 0;
    while (fscanf(fd, "%hd", &val) != EOF && cnt < 350000) {
        uint16_t idx = (uint16_t)(cnt / 7);
        switch (cnt % 7) {
        case 0: gx[idx] = val; break;
        case 1: gy[idx] = val; break;
        case 2: gz[idx] = val; break;
        case 3: ax[idx] = val; break;
        case 4: ay[idx] = val; break;
        case 5: az[idx] = val; break;
        default: break;
        }
        cnt++;
    }
    fclose(fd);

    if (cnt % 7 != 0) return -1;
    *out_len = (uint16_t)(cnt / 7);
    return 0;
}

int main(void)
{
    const char *dirs[]   = { "../../AccData/walk", "../../AccData/run", "../../AccData/others" };
    const char *names[]  = { "Walking", "Running", "Non-walking" };
    int total_files = 0, total_truth = 0;
    float sum_err = 0.0f, sum_mape = 0.0f;
    int mape_files = 0, fp_count = 0, noise_count = 0;

    printf("============================================================\n");
    printf("  VeriHealthi Improved Step Counter - C Test Report\n");
    printf("============================================================\n\n");

    step_counter_improved_init();

    for (int d = 0; d < 3; d++) {
        char cmd[512], filepath[512];
        snprintf(cmd, sizeof(cmd), "ls %s/*.txt 2>/dev/null", dirs[d]);
        FILE *ls = popen(cmd, "r");
        if (!ls) continue;

        printf("--- %s ---\n", names[d]);

        int cat_files = 0, cat_sum_err = 0;
        float cat_sum_mape = 0.0f;
        int cat_mape_cnt = 0;

        while (fgets(filepath, sizeof(filepath), ls)) {
            uint16_t len;
            ImuInput in;
            uint16_t steps;
            char *fn;

            trim_newline(filepath);
            if (!filepath[0]) continue;
            fn = strrchr(filepath, '/');
            fn = fn ? fn + 1 : filepath;

            int truth = extract_truth(fn);

            if (read_imu_file(filepath, ax_buf, ay_buf, az_buf,
                               gx_buf, gy_buf, gz_buf, &len) != 0) {
                continue;
            }

            in.len = len; in.ax = ax_buf; in.ay = ay_buf; in.az = az_buf;
            in.gx = gx_buf; in.gy = gy_buf; in.gz = gz_buf;

            if (step_counter_improved_process(&in, &steps) != ALGO_NORMAL) {
                continue;
            }

            int err = abs((int)steps - truth);
            printf("  %-50s  True=%3d  Pred=%3d  Err=%3d\n", fn, truth, steps, err);

            cat_files++; total_files++;
            total_truth += truth;
            cat_sum_err += err; sum_err += (float)err;

            if (truth > 0) {
                cat_sum_mape += (float)err / (float)truth * 100.0f;
                cat_mape_cnt++;
                sum_mape += (float)err / (float)truth * 100.0f;
                mape_files++;
            } else {
                noise_count++;
                if (steps > 0) fp_count++;
            }
        }

        if (cat_files > 0) {
            printf("\n  MAE: %.2f", (float)cat_sum_err / (float)cat_files);
            if (cat_mape_cnt > 0)
                printf(", MAPE: %.2f%%", cat_sum_mape / (float)cat_mape_cnt);
            printf("\n\n");
        }
        pclose(ls);
    }

    printf("============================================================\n");
    printf("  OVERALL\n");
    printf("  Files: %d, MAE: %.2f\n",
           total_files, total_files > 0 ? sum_err / (float)total_files : 0.0f);
    if (mape_files > 0)
        printf("  MAPE (walk+run): %.2f%%\n", sum_mape / (float)mape_files);
    if (noise_count > 0)
        printf("  FP rate (noise): %d/%d (%.1f%%)\n",
               fp_count, noise_count,
               (float)fp_count / (float)noise_count * 100.0f);
    printf("============================================================\n");

    return 0;
}
