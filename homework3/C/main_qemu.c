/*
 * StepCounter QEMU bare-metal test (Nuclei N307)
 * Reads IMU data file via semihosting, runs algorithm, reports result.
 *
 * Usage: change FILE_PATH below, then ./build.sh && ./qemu.sh
 */
#include "alg_step_counter_improved.h"
#include "soc_init.h"
#include "uart_printf.h"
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

/* Change this to test different data files */
#define FILE_PATH "../AccData/walk/IMU_walk_right_2026_04_29_16_23_14_ID2_step100.txt"

static int extract_truth(const char *path)
{
    const char *p = strstr(path, "_step");
    if (!p) return -1;
    p += 5;
    return atoi(p);
}

/* Read IMU file. Format: 5 header lines, then one int per line,
   7 values per sample: gx gy gz ax ay az debug
   Uses low-level semihosting I/O (open/read/close). */
static int read_imu_file(const char *path,
                          int16_t *gx, int16_t *gy, int16_t *gz,
                          int16_t *ax, int16_t *ay, int16_t *az,
                          uint16_t max_samples)
{
    int fd, cnt = 0, n, line_pos = 0;
    char buf[4096];
    char line[32];
    int has_data = 0;
    int col = 0;      /* 0..6 per sample */
    int16_t tmp[7];   /* temp: gx,gy,gz,ax,ay,az,_ */
    uint16_t i;

    uart_printf("Opening file...\r\n");
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        uart_printf("ERROR: open() returned %d\r\n", fd);
        return -1;
    }
    uart_printf("File opened (fd=%d), reading...\r\n", fd);

    while ((n = read(fd, buf, sizeof(buf))) > 0 && cnt < max_samples) {
        for (i = 0; i < (uint16_t)n && cnt < max_samples; i++) {
            char c = buf[i];
            if (c == '\n') {
                if (line_pos > 0) {
                    line[line_pos] = '\0';
                    if (has_data) {
                        int val = atoi(line);
                        tmp[col] = (int16_t)val;
                        col++;
                        if (col == 7) {
                            gx[cnt] = tmp[0];
                            gy[cnt] = tmp[1];
                            gz[cnt] = tmp[2];
                            ax[cnt] = tmp[3];
                            ay[cnt] = tmp[4];
                            az[cnt] = tmp[5];
                            cnt++;
                            col = 0;
                        }
                    } else if (strstr(line, "TYPE")) {
                        has_data = 1;
                        uart_printf("Found TYPE header, parsing data...\r\n");
                    }
                    line_pos = 0;
                }
            } else if (c != '\r' && line_pos < (int)sizeof(line) - 1) {
                line[line_pos++] = c;
            }
        }
    }
    uart_printf("Read done: %d samples\r\n", cnt);

    close(fd);
    return cnt;
}


/* ============================================================
 * Main
 * ============================================================ */
int main(void)
{
    ImuInput in;
    uint16_t steps = 0;
    int nsamples, truth;
    static int16_t buf_gx[MAX_SAMPLES];
    static int16_t buf_gy[MAX_SAMPLES];
    static int16_t buf_gz[MAX_SAMPLES];
    static int16_t buf_ax[MAX_SAMPLES];
    static int16_t buf_ay[MAX_SAMPLES];
    static int16_t buf_az[MAX_SAMPLES];

    soc_init();
    uart_printf("soc init done\r\n");

    truth = extract_truth(FILE_PATH);
    uart_printf("File: %s\r\n", FILE_PATH);
    uart_printf("True steps: %d\r\n", truth);

    nsamples = read_imu_file(FILE_PATH,
                              buf_gx, buf_gy, buf_gz,
                              buf_ax, buf_ay, buf_az,
                              MAX_SAMPLES);
    if (nsamples <= 0) {
        uart_printf("ERROR: failed to read file (nsamples=%d)\r\n", nsamples);
        while (1) ;
        return 0;
    }
    uart_printf("Read %d samples\r\n", nsamples);

    step_counter_improved_init();
    uart_printf("Step counter initialized\r\n");

    in.len = (uint16_t)nsamples;
    in.ax = buf_ax; in.ay = buf_ay; in.az = buf_az;
    in.gx = buf_gx; in.gy = buf_gy; in.gz = buf_gz;

    if (step_counter_improved_process(&in, &steps) == ALGO_NORMAL) {
        int err = (int)steps - truth;
        if (err < 0) err = -err;
        uart_printf("Result: %d samples -> %d steps detected\r\n",
                     nsamples, steps);
        uart_printf("  True=%d  Pred=%d  Err=%d\r\n",
                     truth, steps, err);
    } else {
        uart_printf("Error processing data\r\n");
    }

    uart_printf("=== Step Counter QEMU Test Complete ===\r\n");
    while (1) ;
    return 0;
}
