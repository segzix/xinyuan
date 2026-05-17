#ifndef MESSAGE_DEFS_H
#define MESSAGE_DEFS_H

#include <stdint.h>

/* Message codes */
#define MSG_CODE_HEARTBEAT       0x0001
#define MSG_CODE_TEMPERATURE     0x0002
#define MSG_CODE_BATTERY_LEVEL   0x0003
#define MSG_CODE_HEART_RATE      0x0004
#define MSG_CODE_STEP_COUNT      0x0005
#define MSG_CODE_BLOOD_OXYGEN    0x0006
#define MSG_CODE_SLEEP_STATE     0x0007
#define MSG_CODE_ACCEL_DATA      0x0008
#define MSG_CODE_GYRO_DATA       0x0009
#define MSG_CODE_CALORIES        0x000A
#define MSG_CODE_DISTANCE        0x000B
#define MSG_CODE_BP_SYSTOLIC     0x000C
#define MSG_CODE_BP_DIASTOLIC    0x000D
#define MSG_CODE_ECG_SAMPLE      0x000E
#define MSG_CODE_FALL_DETECT     0x000F
#define MSG_CODE_CHARGER_STATE   0x0010
#define MSG_CODE_ALARM_EVENT     0x0011
#define MSG_CODE_FIRMWARE_VERSION 0x0012
#define MSG_CODE_SENSOR_STATE    0x0013
#define MSG_CODE_USER_ACTIVITY   0x0014
#define MSG_CODE_PPG_RAW         0x0015
#define MSG_CODE_SKIN_TEMP       0x0016
#define MSG_CODE_HUMIDITY        0x0017
#define MSG_CODE_UV_INDEX        0x0018
#define MSG_CODE_AIR_QUALITY     0x0019
#define MSG_CODE_MOTION_MODE     0x001A
#define MSG_CODE_WATER_INTAKE    0x001B
#define MSG_CODE_STRESS_LEVEL    0x001C
#define MSG_CODE_RESP_RATE       0x001D
#define MSG_CODE_HRV             0x001E

typedef struct {
    uint32_t code;
    const char *name;
    const char *description;
} MessageTranslation;

extern const int msg_table_count;

const char *translate_message(uint32_t code);
const MessageTranslation *translate_message_entry(uint32_t code);

#endif /* MESSAGE_DEFS_H */
