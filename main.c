/*
 * Copyright (c) 2026, VeriSilicon Holdings Co., Ltd. All rights reserved
 *
 * 1. Redistributing the source code of this software is only allowed after
 * receiving explicit, written permission from VeriSilicon. The copyright notice,
 * this list of conditions and the following disclaimer must be retained in all
 * source code distributions.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 */

//#include <stddef.h>
//#include <stdint.h>
//#include "vs_conf.h"
//#include "soc_init.h"
//#include "soc_sysctl.h"
//#include "bsp.h"
//#include "uart_printf.h"
//#include "board.h"
//#include "osal.h"
//#include "vpi_error.h"
//#include "main.h"
//
//static void task_sample(void *param)
//{
//    int count = 0;
//
//    while (count < 10) {
//        count++;
//        uart_printf("Sample task count %d\r\n", count);
//        osal_sleep(1000);
//    }
//
//    uart_printf("Finish sample task!\r\n");
//    osal_delete_task(NULL);
//}
//
//static void task_init_app(void *param)
//{
//    int ret;
//    BoardDevice board_dev;
//
//    ret = board_register(board_get_ops());
//    ret = vsd_to_vpi(ret);
//    if (ret != VPI_SUCCESS) {
//        uart_printf("board register failed %d", ret);
//        goto exit;
//    }
//    ret = board_init((void *)&board_dev);
//    ret = vsd_to_vpi(ret);
//    if (ret != VPI_SUCCESS) {
//        uart_printf("board init failed %d", ret);
//        goto exit;
//    }
//    if (board_dev.name) {
//        uart_printf("Board: %s", board_dev.name);
//    }
//
//    uart_printf("Hello VeriHealthi!\r\n");
//
//    osal_create_task(task_sample, "task_sample", 512, 4, NULL);
//exit:
//    osal_delete_task(NULL);
//}
//
//int main(void)
//{
//    int ret;
//
//    ret = soc_init();
//    ret = vsd_to_vpi(ret);
//    if (ret != VPI_SUCCESS) {
//        uart_printf("soc init error %d", ret);
//        goto exit;
//    } else {
//        uart_printf("soc init done");
//    }
//    osal_pre_start_scheduler();
//    osal_create_task(task_init_app, "init_app", 512, 1, NULL);
//    osal_start_scheduler();
//exit:
//    goto exit;
//}
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "vs_conf.h"
#include "soc_init.h"
#include "soc_sysctl.h"
#include "bsp.h"
#include "uart_printf.h"
#include "board.h"
#include "osal.h"
#include "vpi_error.h"
#include "vpi_event.h"
#include "hal_imu.h"
#include "main.h"

#define IMU_SAMPLE_HZ          50
#define IMU_READ_FRAMES        16

#define CRC_TARGET_BYTES       32000u
#define IMU_BUF_FRAMES         ((CRC_TARGET_BYTES / sizeof(ImuGyroAccelData)) + 64u)

/* 50 Hz 下，50 帧约 1 秒 */
#define ALGO_WIN_FRAMES        50
#define ALGO_COOLDOWN_FRAMES   75

/*
 * 这里复用 SDK 已有 EVENT_ALGO_SW_TIMEOUT 作为“通知算法处理新数据”的事件。
 * 正式工程也可以扩展一个自定义 EventId，例如 EVENT_IMU_ALGO_PROCESS。
 */
#define EVENT_IMU_ALGO_PROCESS EVENT_ALGO_SW_TIMEOUT

static ImuDevice *g_imu_dev = NULL;

static ImuGyroAccelData g_imu_buf[IMU_BUF_FRAMES];
static volatile uint32_t g_imu_write_frames = 0;

static uint32_t g_algo_read_frames = 0;

/* CRC32/IEEE: poly = 0xEDB88320 */
static uint32_t g_crc = 0xFFFFFFFFu;
static uint32_t g_crc_bytes = 0;
static bool g_crc_printed = false;

/* 算法滑动窗口 */
static ImuGyroAccelData g_win[ALGO_WIN_FRAMES];
static uint32_t g_win_wr = 0;
static uint32_t g_win_cnt = 0;
static uint32_t g_total_algo_frames = 0;
static uint32_t g_last_emit_frame = 0;

static int32_t abs_i32(int32_t x)
{
    return x >= 0 ? x : -x;
}

static int32_t max_i32(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

static uint32_t crc32_update_byte(uint32_t crc, uint8_t data)
{
    uint32_t i;

    crc ^= data;
    for (i = 0; i < 8; i++) {
        if (crc & 1u) {
            crc = (crc >> 1) ^ 0xEDB88320u;
        } else {
            crc >>= 1;
        }
    }

    return crc;
}

static void crc32_update_data(const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;

    while ((len > 0) && (g_crc_bytes < CRC_TARGET_BYTES)) {
        g_crc = crc32_update_byte(g_crc, *p);
        p++;
        len--;
        g_crc_bytes++;
    }

    if ((!g_crc_printed) && (g_crc_bytes >= CRC_TARGET_BYTES)) {
        uint32_t crc_final = g_crc ^ 0xFFFFFFFFu;
        g_crc_printed = true;
        uart_printf("CRC32(320000Byte)=0x%08x\r\n", (unsigned int)crc_final);
    }
}

static void algo_emit_result(const char *label)
{
    uint32_t now_ms;

    now_ms = (g_total_algo_frames * 1000u) / IMU_SAMPLE_HZ;
    uart_printf("%dms, %s\r\n", (int)now_ms, label);

    g_last_emit_frame = g_total_algo_frames;
}

/*
 * 简单阈值算法：
 * 1. up/down：1 秒窗口内 x 轴角速度积分很大，且 y 轴加速度变化明显；
 * 2. clench：短时冲击强，gyro 峰值和加速度范围明显大于 pinch；
 * 3. pinch：冲击较弱，但仍高于静止噪声；
 * 4. others：不打印。
 *
 * 注意：阈值需要结合 VeriHealthi_IMU_Dataset 继续调参。
 */
static void algo_detect_one_sample(const ImuGyroAccelData *s)
{
    int i;
    int idx;

    int32_t gx_sum = 0;
    int32_t gy_sum = 0;
    int32_t gz_sum = 0;

    int32_t g_peak = 0;

    int32_t ax_min = 32767;
    int32_t ax_max = -32768;
    int32_t ay_min = 32767;
    int32_t ay_max = -32768;
    int32_t az_min = 32767;
    int32_t az_max = -32768;

    int32_t ax_rng;
    int32_t ay_rng;
    int32_t az_rng;
    int32_t acc_rng_sum;

    const char *label = NULL;

    g_win[g_win_wr] = *s;
    g_win_wr++;
    if (g_win_wr >= ALGO_WIN_FRAMES) {
        g_win_wr = 0;
    }

    if (g_win_cnt < ALGO_WIN_FRAMES) {
        g_win_cnt++;
    }

    g_total_algo_frames++;

    if (g_win_cnt < ALGO_WIN_FRAMES) {
        return;
    }

    if ((g_total_algo_frames - g_last_emit_frame) < ALGO_COOLDOWN_FRAMES) {
        return;
    }

    /*
     * g_win_wr 指向下一个写入位置，也就是当前窗口中最老数据的位置。
     */
    for (i = 0; i < ALGO_WIN_FRAMES; i++) {
        ImuGyroAccelData *w;

        idx = (int)((g_win_wr + i) % ALGO_WIN_FRAMES);
        w = &g_win[idx];

        gx_sum += w->gx;
        gy_sum += w->gy;
        gz_sum += w->gz;

        g_peak = max_i32(g_peak, abs_i32(w->gx));
        g_peak = max_i32(g_peak, abs_i32(w->gy));
        g_peak = max_i32(g_peak, abs_i32(w->gz));

        if (w->ax < ax_min) ax_min = w->ax;
        if (w->ax > ax_max) ax_max = w->ax;

        if (w->ay < ay_min) ay_min = w->ay;
        if (w->ay > ay_max) ay_max = w->ay;

        if (w->az < az_min) az_min = w->az;
        if (w->az > az_max) az_max = w->az;
    }

    ax_rng = ax_max - ax_min;
    ay_rng = ay_max - ay_min;
    az_rng = az_max - az_min;
    acc_rng_sum = ax_rng + ay_rng + az_rng;

    /*
     * up/down：手腕翻转动作明显，gx_sum 正负可区分抬腕/放下。
     * 这里的方向基于给定数据集统计结果；若设备佩戴方向变化，需要重新校正符号。
     */
    if ((abs_i32(gx_sum) > 28000) && (ay_rng > 2500)) {
        if (gx_sum > 0) {
            label = "up";
        } else {
            label = "down";
        }
    }
    /*
     * clench：握拳冲击强，角速度峰值和加速度扰动都明显。
     */
    else if ((g_peak > 1100) &&
             (acc_rng_sum > 5500) &&
             (abs_i32(gx_sum) < 25000)) {
        label = "clench";
    }
    /*
     * pinch：双指互点冲击较轻，窗口能量低于握拳。
     */
    else if ((g_peak > 280) &&
             (acc_rng_sum > 1600) &&
             (acc_rng_sum < 5500) &&
             (abs_i32(gx_sum) < 16000)) {
        label = "pinch";
    }

    if (label != NULL) {
        algo_emit_result(label);
    }
}

static void algo_process_new_data(void)
{
    uint32_t end_frames;

    osal_enter_critical();
    end_frames = g_imu_write_frames;
    osal_exit_critical();

    while (g_algo_read_frames < end_frames) {
        algo_detect_one_sample(&g_imu_buf[g_algo_read_frames]);
        g_algo_read_frames++;
    }
}

static int algo_event_handler(EventManager manager, EventId event_id, EventParam param)
{
    (void)manager;
    (void)param;

    if (event_id == EVENT_IMU_ALGO_PROCESS) {
        algo_process_new_data();
    }

    return EVENT_OK;
}

static void algo_task(void *param)
{
    EventManager algo_mgr;

    (void)param;

    algo_mgr = vpi_event_new_manager(EVENT_MGR_ALGO, algo_event_handler);
    if (algo_mgr == NULL) {
        uart_printf("create algo manager failed\r\n");
        osal_delete_task(NULL);
        return;
    }

    vpi_event_register(EVENT_IMU_ALGO_PROCESS, algo_mgr);

    while (1) {
        vpi_event_listen(algo_mgr);
    }
}

static void imu_data_ready_isr(void)
{
    vpi_event_notify_from_isr(EVENT_SEN_DATA_READY, NULL);
}

static int imu_check_ret(const char *step, int ret)
{
    if (ret != 0) {
        uart_printf("%s failed, ret=%d\r\n", step, ret);
        return -1;
    }

    return 0;
}

static int imu_hw_init(void)
{
    int ret;
    uint8_t cfg_mask;

    g_imu_dev = hal_imu_get_device(IMU_DEV_ID_0);
    if (g_imu_dev == NULL) {
        uart_printf("hal_imu_get_device failed\r\n");
        return -1;
    }

    /*
     * 某些平台要求先 enable_power，再 init。
     */
    ret = hal_imu_enable_power(g_imu_dev, true);
    if (imu_check_ret("hal_imu_enable_power", ret) != 0) return -1;

    ret = hal_imu_init(g_imu_dev);
    if (imu_check_ret("hal_imu_init", ret) != 0) return -1;

    ret = hal_imu_set_sensor_default_cfg(g_imu_dev);
    if (imu_check_ret("hal_imu_set_sensor_default_cfg", ret) != 0) return -1;

    cfg_mask = IMU_SENSOR_RANGE | IMU_SENSOR_ODR | IMU_SENSOR_BWP;
    //cfg_mask = IMU_SENSOR_ODR;
    /*
     * 加速度计：±8G，50Hz。
     * bwp 这里先给 0，若当前 SDK/驱动对 bwp 有具体枚举要求，需要按 hal_imu.h/API 文档调整。
     */
    ret = hal_imu_set_accel_cfg(g_imu_dev, 8, 20, IMU_SAMPLE_HZ, cfg_mask);
    if (imu_check_ret("hal_imu_set_accel_cfg", ret) != 0) return -1;

    /*
     * 陀螺仪：±2000 dps，50Hz。
     */
    ret = hal_imu_set_gyro_cfg(g_imu_dev, 2000, 20, IMU_SAMPLE_HZ, cfg_mask);
    if (imu_check_ret("hal_imu_set_gyro_cfg", ret) != 0) return -1;

    ret = hal_imu_set_work_mode(g_imu_dev, IMU_ACCEL_GYRO, IMU_SEN_MODE_NORMAL);
    if (imu_check_ret("hal_imu_set_work_mode", ret) != 0) return -1;

    /*
     * FIFO watermark：SDK 推荐值为 49。
     */
    ret = hal_imu_set_fifo_wm(g_imu_dev, FIFO_WATERMARK_LEVEL);
    if (imu_check_ret("hal_imu_set_fifo_wm", ret) != 0) return -1;

    ret = hal_imu_set_fifo_cfg(g_imu_dev, IMU_FIFO_ACCEL | IMU_FIFO_GYRO | IMU_FIFO_TIME, true);
    if (imu_check_ret("hal_imu_set_fifo_cfg", ret) != 0) return -1;

    ret = hal_imu_flush_fifo(g_imu_dev);
    if (imu_check_ret("hal_imu_flush_fifo", ret) != 0) return -1;

    /*
     * QEMU IMU 模拟器只支持 IMU_ACC_GYRO_FIFO_WATERMARK_INTERRUPT。
     */
    ret = hal_imu_cfg_interrupt(g_imu_dev,
                                true,
                                IMU_ACC_GYRO_FIFO_WATERMARK_INTERRUPT,
                                NULL);
    if (imu_check_ret("hal_imu_cfg_interrupt", ret) != 0) return -1;

    ret = hal_imu_enable_interrupt(g_imu_dev,
                                   IMU_DATA_PIN,
                                   true,
                                   imu_data_ready_isr);
    if (imu_check_ret("hal_imu_enable_interrupt", ret) != 0) return -1;

    uart_printf("IMU init done, sample_rate=%dHz\r\n", IMU_SAMPLE_HZ);
    return 0;
}

static int imu_event_handler(EventManager manager, EventId event_id, EventParam param)
{
    int ret;
    int i;
    uint16_t available = 0;
    ImuGyroAccelData frames[IMU_READ_FRAMES];

    (void)manager;
    (void)param;

    if (event_id != EVENT_SEN_DATA_READY) {
        return EVENT_OK;
    }

    memset(frames, 0, sizeof(frames));
    ret = hal_imu_get_fifo_data(g_imu_dev);
    ret = hal_imu_read_gyro_accel(g_imu_dev,
                                  frames,
                                  IMU_READ_FRAMES,
                                  &available);

    if ((ret == 0) && (available > 0)) {
        for (i = 0; i < available; i++) {
            if (g_imu_write_frames < IMU_BUF_FRAMES) {
                g_imu_buf[g_imu_write_frames] = frames[i];

                crc32_update_data(&frames[i], sizeof(ImuGyroAccelData));

                osal_enter_critical();
                g_imu_write_frames++;
                osal_exit_critical();
            }
        }

        /*
         * 通知 algo_task 处理新数据。
         */
        vpi_event_notify(EVENT_IMU_ALGO_PROCESS, NULL);
    } else {
        uart_printf("hal_imu_read_gyro_accel ret=%d, available=%d\r\n",
                    ret,
                    available);
    }

    /*
     * data ready 中断为电平触发时，读完 FIFO 后需要重新使能。
     */
    hal_imu_enable_interrupt(g_imu_dev,
                             IMU_DATA_PIN,
                             true,
                             imu_data_ready_isr);

    return EVENT_OK;
}

static void imu_task(void *param)
{
    EventManager imu_mgr;

    (void)param;

    imu_mgr = vpi_event_new_manager(EVENT_MGR_SEN, imu_event_handler);
    if (imu_mgr == NULL) {
        uart_printf("create imu manager failed\r\n");
        osal_delete_task(NULL);
        return;
    }

    vpi_event_register(EVENT_SEN_DATA_READY, imu_mgr);

    /*
     * 等 algo_task 先完成 manager 注册，避免最开始的处理事件丢失。
     */
    osal_sleep(100);

    if (imu_hw_init() != 0) {
        uart_printf("IMU hw init failed\r\n");
        osal_delete_task(NULL);
        return;
    }

    while (1) {
        vpi_event_listen(imu_mgr);
    }
}

static void task_init_app(void *param)
{
    int ret;
    BoardDevice board_dev;

    (void)param;
    memset(&board_dev, 0, sizeof(board_dev));

    ret = board_register(board_get_ops());
    ret = vsd_to_vpi(ret);
    if (ret != VPI_SUCCESS) {
        uart_printf("board register failed %d\r\n", ret);
        goto exit;
    }

    ret = board_init((void *)&board_dev);
    ret = vsd_to_vpi(ret);
    if (ret != VPI_SUCCESS) {
        uart_printf("board init failed %d\r\n", ret);
        goto exit;
    }

    if (board_dev.name) {
        uart_printf("Board: %s\r\n", board_dev.name);
    }

    uart_printf("Hello VeriHealthi IMU Gesture Demo!\r\n");

    /*
     * Event queue size 必须在 register event 之前设置。
     */
    vpi_event_set_queue_size(32);

    osal_create_task(algo_task, "algo_task", 2048, 4, NULL);
    osal_create_task(imu_task,  "imu_task",  2048, 5, NULL);

exit:
    osal_delete_task(NULL);
}

int main(void)
{
    int ret;

    ret = soc_init();
    ret = vsd_to_vpi(ret);

    if (ret != VPI_SUCCESS) {
        uart_printf("soc init error %d\r\n", ret);
        goto exit;
    } else {
        uart_printf("soc init done\r\n");
    }

    osal_pre_start_scheduler();
    osal_create_task(task_init_app, "init_app", 1024, 1, NULL);
    osal_start_scheduler();

exit:
    while (1) {
    }
}
