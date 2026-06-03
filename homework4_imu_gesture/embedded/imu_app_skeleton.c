/*
 * VeriHealthi SDK IMU gesture application skeleton.
 *
 * This file documents the intended embedded integration points. Replace the
 * TODO SDK HAL stubs with the IMU HAL names from the preliminary SDK manual.
 */
#include "algo_manager.h"
#include "crc8_smbus.h"
#include "imu_buffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "board.h"
#include "bsp.h"
#include "osal.h"
#include "soc_init.h"
#include "uart_printf.h"
#include "vpi_error.h"
#include "vpi_event.h"
#include "vpi_event_def.h"
#include "vs_conf.h"

static OsalSemaphore *g_imu_ready_sem;
static ImuBuffer g_imu_buffer;
static AlgoManager g_algo_manager;
static uint8_t g_crc8;
static uint32_t g_crc_bytes;
static bool g_crc_printed;

static void print_gesture(uint32_t time_ms, GestureResult result, void *user_data)
{
    (void)user_data;
    if (result == GESTURE_OTHER) {
        return;
    }
    uart_printf("%ums, %s\r\n", (unsigned int)time_ms, gesture_result_name(result));
}

/* ISR rule: do not call HAL, non-ISR-safe OS/Event APIs, CRC, or printf here. */
void imu_data_ready_isr(void)
{
    if (g_imu_ready_sem != NULL) {
        (void)osal_sem_post_isr(g_imu_ready_sem);
    }
}

static int imu_hal_config_50hz(void)
{
    /* TODO SDK HAL: get IMU device, init it, set accel/gyro ODR to 50Hz,
     * enable data-ready interrupt, and register imu_data_ready_isr(). */
    return VPI_SUCCESS;
}

static int imu_hal_read_sample(ImuGyroAccelData *sample)
{
    if (sample == NULL) {
        return VPI_ERROR;
    }

    /* TODO SDK HAL: read one complete ImuGyroAccelData from IMU HAL. */
    sample->gx = 0;
    sample->gy = 0;
    sample->gz = 0;
    sample->ax = 0;
    sample->ay = 0;
    sample->az = 0;
    sample->debug = 0;
    return VPI_SUCCESS;
}

static void update_first_320000_bytes_crc(const ImuGyroAccelData *sample)
{
    uint32_t remain;
    uint32_t update;

    if (g_crc_printed || sample == NULL) {
        return;
    }

    remain = IMU_GESTURE_CRC_TARGET_BYTES - g_crc_bytes;
    /* The required byte boundary may split the final sample. */
    update = IMU_GESTURE_SAMPLE_BYTES < remain ? IMU_GESTURE_SAMPLE_BYTES : remain;
    g_crc8 = crc8_smbus_update(g_crc8, sample, update);
    g_crc_bytes += update;

    if (g_crc_bytes == IMU_GESTURE_CRC_TARGET_BYTES) {
        uart_printf("CRC8_SMBUS(320000B)=0x%02X\r\n", g_crc8);
        g_crc_printed = true;
    }
}

static void imu_task(void *param)
{
    (void)param;

    if (imu_hal_config_50hz() != VPI_SUCCESS) {
        uart_printf("imu init failed\r\n");
        osal_delete_task(NULL);
        return;
    }

    while (1) {
        ImuGyroAccelData sample;
        ImuBlock block;

        if (osal_sem_wait(g_imu_ready_sem, OSAL_WAIT_FOREVER) != OSAL_SUCCESS) {
            continue;
        }
        if (imu_hal_read_sample(&sample) != VPI_SUCCESS) {
            continue;
        }

        update_first_320000_bytes_crc(&sample);
        if (imu_buffer_push(&g_imu_buffer, &sample, &block)) {
            ImuBlock *event_block = imu_buffer_block_descriptor(&g_imu_buffer, &block);

            if (event_block != NULL) {
                (void)vpi_event_notify(EVENT_SEN_DATA_READY, (EventParam)event_block);
            }
        }
    }
}

static int algo_event_handler(EventManager manager, EventId event_id, EventParam param)
{
    ImuBlock *block = (ImuBlock *)param;

    (void)manager;
    if (event_id != EVENT_SEN_DATA_READY || block == NULL) {
        return VPI_SUCCESS;
    }

    algo_manager_process(&g_algo_manager, block);
    imu_buffer_release(&g_imu_buffer, block);
    return VPI_SUCCESS;
}

static void algo_task(void *param)
{
    EventManager manager;
    int ret;

    (void)param;
    manager = vpi_event_new_manager(EVENT_MGR_ALGO, algo_event_handler);
    if (manager == NULL) {
        uart_printf("algo manager create failed\r\n");
        osal_delete_task(NULL);
        return;
    }

    ret = vpi_event_register(EVENT_SEN_DATA_READY, manager);
    if (vsd_to_vpi(ret) != VPI_SUCCESS) {
        uart_printf("algo event register failed %d\r\n", ret);
        osal_delete_task(NULL);
        return;
    }

    while (1) {
        (void)vpi_event_listen(manager);
    }
}

static void task_init_app(void *param)
{
    BoardDevice board_dev;
    int ret;

    (void)param;
    ret = board_register(board_get_ops());
    if (vsd_to_vpi(ret) != VPI_SUCCESS) {
        uart_printf("board register failed %d\r\n", ret);
        goto exit;
    }

    ret = board_init((void *)&board_dev);
    if (vsd_to_vpi(ret) != VPI_SUCCESS) {
        uart_printf("board init failed %d\r\n", ret);
        goto exit;
    }

    imu_buffer_init(&g_imu_buffer);
    algo_manager_init(&g_algo_manager, print_gesture, NULL);
    if (osal_create_counting_sem(&g_imu_ready_sem, 255u, 0u) != OSAL_SUCCESS) {
        uart_printf("imu semaphore create failed\r\n");
        goto exit;
    }

    osal_create_task(algo_task, "algo_task", 768, 3, NULL);
    osal_create_task(imu_task, "imu_task", 768, 4, NULL);

exit:
    osal_delete_task(NULL);
}

int main(void)
{
    int ret;

    ret = soc_init();
    if (vsd_to_vpi(ret) != VPI_SUCCESS) {
        uart_printf("soc init error %d\r\n", ret);
        while (1) {
        }
    }

    osal_pre_start_scheduler();
    osal_create_task(task_init_app, "init_app", 512, 1, NULL);
    osal_start_scheduler();

    while (1) {
    }
}
