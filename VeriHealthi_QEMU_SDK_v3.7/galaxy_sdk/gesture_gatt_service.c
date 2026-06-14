/*
 * gesture_gatt_service.c
 *
 *  Created on: 2026年6月12日
 *      Author: meng mingyue
 */

#include <vsbt_config.h>
#include <errno.h>
#include <stdbool.h>

#include <bluetooth.h>
#include <conn.h>
#include <gatt.h>
#include <uuid.h>
#include "gesture_service_uuid.h"

#define LOG_MODULE_NAME "gesture"

static char gesture[GESTURE_CHAR_SIZE];

static void gesture_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	(void)(attr);
	__unused bool notif_enabled = (value == BT_GATT_CCC_NOTIFY);
}

static ssize_t read_gesture(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                         uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, gesture, 8);
}

BT_GATT_SERVICE_DEFINE(ges, BT_GATT_PRIMARY_SERVICE(BT_UUID_GES1),
                       BT_GATT_CHARACTERISTIC(BT_UUID_GES2,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_INDICATE,
                                              BT_GATT_PERM_READ_ENCRYPT, read_gesture, NULL,
                                              &gesture),
					   BT_GATT_CCC(gesture_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),);

int bt_bas_set_gesture_char(char* gest)
{
	int rc;

    memcpy(gesture, gest, GESTURE_CHAR_SIZE);

    rc = bt_gatt_notify(NULL, &ges.attrs[1], gesture, GESTURE_CHAR_SIZE);

    return rc == -ENOTCONN ? 0 : rc;
}

