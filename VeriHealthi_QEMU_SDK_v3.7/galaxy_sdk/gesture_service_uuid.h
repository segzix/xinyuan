#ifndef __UUID_HEADER__
#define __UUID_HEADER__
#include <uuid.h>

#define GESTURE_SERVICE_UUID \
	BT_UUID_128_ENCODE(0x00000001, 0x0004, 0x1000, 0x8000, 0x00805F9B05B5)
#define GESTURE_CHAR_UUID \
	BT_UUID_128_ENCODE(0x00000002, 0x0004, 0x1000, 0x8000, 0x00805F9B05B5)

#define BT_UUID_GES1 \
	BT_UUID_DECLARE_128(GESTURE_SERVICE_UUID)
#define BT_UUID_GES2 \
	BT_UUID_DECLARE_128(GESTURE_CHAR_UUID)

#define GESTURE_CHAR_SIZE	  8

int bt_bas_set_gesture_char(char* gest);
void ble_app_start(void);

#endif
