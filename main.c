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

#include <stddef.h>
#include <stdint.h>
#include "vs_conf.h"
#include "soc_init.h"
#include "soc_sysctl.h"
#include "bsp.h"
#include "uart_printf.h"
#include "board.h"
#include "osal.h"
#include "vpi_error.h"
#include "vpi_event.h"
#include "main.h"

/* 自定义事件 ID */
#define EVENT_TRANS      0x1001
/* 自定义 Event Manager ID */
#define RECIEVE_MANAGER_ID      1
#define SEND_MANAGER_ID     	2

/* Event Manager 句柄 */
static void *pReceiveManager = 0;
static void *pSendManager = 0;

/**************************************
 * 任务1的事件处理函数
 * 作用：收到 EVENT_SYS_TEST 后，解析参数并打印
 */
static int recieve_handler(EventManager manager, EventId event_id, EventParam param)
{
	unsigned int value = 0;

    if (event_id == EVENT_TRANS)
    {
        value = *(unsigned int *)param;
        uart_printf("[Task1] Get value = 0x%X\r\n", value);
    }

    return event_id;
}

/*
 * 任务1：监听消息
 */
static void receive_task(void *arg)
{
	uart_printf("[Task1] receiving task start\r\n");

    /* 创建接收事件管理器*/
    pReceiveManager = vpi_event_new_manager(RECIEVE_MANAGER_ID, recieve_handler);

    if (pReceiveManager == 0)
    {
    	uart_printf("[Task1] Create receiving manager failed\r\n");
        return;
    }

    /* 注册需要监听的事件*/
    vpi_event_register(EVENT_TRANS, pReceiveManager);

    uart_printf("[Task1] Register EVENT_TRANS success\r\n");

    /* 一直监听事件*/
    while (1)
    {
        vpi_event_listen(pReceiveManager);
    }
}

/****************************************
 * 任务2：发送消息
 */
static void send_task(void *arg)
{
	unsigned int send_value = 0xA5A5;

	uart_printf("[Task2] sending task start\r\n");

    /* 创建自定义事件管理器*/
    pSendManager = vpi_event_new_manager(SEND_MANAGER_ID, 0);

    if (pSendManager == 0)
    {
    	uart_printf("[Task2] Create sending manager failed\r\n");
        return;
    }

    /* 延时 1 秒，保证任务1已经注册并开始监听*/
    osal_sleep(1000);

    /* 发送事件 EVENT_SYS_TEST，并携带参数 send_value*/
    uart_printf("[Task2] Send EVENT_TRANS, value = 0x%X\r\n", send_value);
    vpi_event_notify(EVENT_TRANS, (EventParam)&send_value);

    /* 任务2发送完后保持运行*/
    while (1)
    {
        osal_sleep(1000);
    }
}

static void task_init_app(void *param)
{
    int ret;
    BoardDevice board_dev;

    ret = board_register(board_get_ops());
    ret = vsd_to_vpi(ret);
    if (ret != VPI_SUCCESS) {
        uart_printf("board register failed %d", ret);
        goto exit;
    }
    ret = board_init((void *)&board_dev);
    ret = vsd_to_vpi(ret);
    if (ret != VPI_SUCCESS) {
        uart_printf("board init failed %d", ret);
        goto exit;
    }
    if (board_dev.name) {
        uart_printf("Board: %s", board_dev.name);
    }

    osal_create_task(receive_task, "receive_task", 512, 4, NULL);
    osal_create_task(send_task, "send_task", 512, 4, NULL);
exit:
    osal_delete_task(NULL);
}

int main(void)
{
    int ret;

    ret = soc_init();
    ret = vsd_to_vpi(ret);
    if (ret != VPI_SUCCESS) {
        uart_printf("soc init error %d", ret);
        goto exit;
    } else {
        uart_printf("soc init done");
    }
    osal_pre_start_scheduler();
    osal_create_task(task_init_app, "init_app", 512, 1, NULL);
    osal_start_scheduler();
    while(1){}
exit:
    goto exit;
}
