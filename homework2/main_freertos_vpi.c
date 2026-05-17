/*
 * translate_message — FreeRTOS + VPI 事件驱动版本
 *
 * 原始设计: send_task 通过 VPI 事件系统发送消息,
 * receive_task 监听并调用 receive_handler 查表翻译。
 *
 * 需要完整 xxldsp DSP 支持的 QEMU (QEMU 2024.06 仅部分支持,
 * 不含 maddr32 指令) 或不含 DSP 的 SDK 预编译库。
 *
 * 用法: 替换 main.c 后编译运行。
 *   cp main_freertos_vpi.c main.c && ./build.sh && ./run_qemu.sh
 */

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
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
#include "vpi_event_def.h"
#include "message_defs.h"

/* Exception handler registration from bsp library */
extern int system_exception_register(int exc_code, void *handler);

/* DSP software emulation — 仅在 QEMU 无完整 xxldsp 时需要 */
extern int dsp_emu_illegal_handler(uintptr_t mcause, uint32_t *ctx);

static int receive_handler(EventManager manager, EventId event_id, EventParam param)
{
    AppData *app_data = (AppData *)param;

    if (app_data == NULL || app_data->data == NULL) {
        uart_printf("[TRANSLATE] Invalid message param\r\n");
        return VPI_SUCCESS;
    }
    if (app_data->size < sizeof(uint32_t)) {
        uart_printf("[TRANSLATE] Message too small\r\n");
        return VPI_SUCCESS;
    }

    uint32_t msg_code = *(uint32_t *)app_data->data;

    const MessageTranslation *entry = translate_message_entry(msg_code);
    const char *translated = entry ? entry->name : NULL;
    const char *description = entry ? entry->description : NULL;

    if (translated != NULL) {
        uart_printf("[TRANSLATE] 0x%04" PRIx32 " -> %-18s : %s\r\n",
                     msg_code, translated, description);
    } else {
        uart_printf("[TRANSLATE] 0x%04" PRIx32 " -> UNKNOWN_CODE\r\n", msg_code);
    }

    return VPI_SUCCESS;
}

static void send_task(void *param)
{
    static const uint32_t codes[] = {
        MSG_CODE_HEARTBEAT,
        MSG_CODE_HEART_RATE,
        MSG_CODE_STEP_COUNT,
        MSG_CODE_TEMPERATURE,
        MSG_CODE_BATTERY_LEVEL,
    };
    int msg_count = sizeof(codes) / sizeof(codes[0]);

    uart_printf("[SEND] Task started\r\n");
    osal_sleep(500);

    for (int i = 0; i < msg_count; i++) {
        AppData app_data;
        uint32_t code = codes[i];

        memset(&app_data, 0, sizeof(app_data));
        app_data.ack.id = i;
        app_data.ack.error = VPI_SUCCESS;
        app_data.size = sizeof(uint32_t);
        app_data.data = &code;

        uart_printf("[SEND] #%d -> code=0x%04" PRIx32 "\r\n", i, code);
        int ret = vpi_event_notify(EVENT_SYS_TEST, (EventParam)&app_data);
        if (ret != EVENT_OK) {
            uart_printf("[SEND] notify failed: %d\r\n", ret);
        }

        osal_sleep(1200);
    }

    uart_printf("[SEND] Task finished\r\n");
    osal_sleep(1000);
    uart_printf("=== Message Translation Demo Complete ===\r\n");
    osal_delete_task(NULL);
}

static void receive_task(void *param)
{
    int ret;

    uart_printf("[RECV] Task started\r\n");

    EventManager mgr = vpi_event_new_manager(EVENT_MGR_CUSTOM, receive_handler);
    if (mgr == NULL) {
        uart_printf("[RECV] Failed to create event manager\r\n");
        osal_delete_task(NULL);
        return;
    }

    ret = vpi_event_register(EVENT_SYS_TEST, mgr);
    ret = vsd_to_vpi(ret);
    if (ret != VPI_SUCCESS) {
        uart_printf("[RECV] Failed to register event: %d\r\n", ret);
        osal_delete_task(NULL);
        return;
    }

    uart_printf("[RECV] Listening for messages...\r\n");

    while (1) {
        ret = vpi_event_listen(mgr);
        ret = vsd_to_vpi(ret);
        if (ret == VPI_SUCCESS) {
            uart_printf("[RECV] Event processed\r\n");
        } else {
            uart_printf("[RECV] Listen error: %d\r\n", ret);
            break;
        }
    }

    osal_delete_task(NULL);
}

static void task_init_app(void *param)
{
    int ret;
    BoardDevice board_dev;

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

    uart_printf("=== Message Translation Demo ===\r\n");

    osal_create_task(receive_task, "receive", 512, 3, NULL);
    osal_create_task(send_task, "send", 512, 2, NULL);

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
    }
    uart_printf("soc init done\r\n");

    /* 仅在 QEMU 无完整 xxldsp 支持时需要注册 DSP 模拟器 */
    system_exception_register(2, (void *)dsp_emu_illegal_handler);
    uart_printf("DSP emulation enabled\r\n");

    osal_pre_start_scheduler();
    osal_create_task(task_init_app, "init_app", 512, 1, NULL);
    osal_start_scheduler();
exit:
    while (1) ;
}
