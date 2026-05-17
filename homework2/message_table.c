#include <stddef.h>
#include "message_defs.h"

static const MessageTranslation msg_table[] = {
    { MSG_CODE_HEARTBEAT,       "HEARTBEAT",        "System heartbeat signal" },
    { MSG_CODE_TEMPERATURE,     "TEMPERATURE",      "Body temperature measurement" },
    { MSG_CODE_BATTERY_LEVEL,   "BATTERY_LEVEL",    "Battery level percentage" },
    { MSG_CODE_HEART_RATE,      "HEART_RATE",       "Real-time heart rate (BPM)" },
    { MSG_CODE_STEP_COUNT,      "STEP_COUNT",       "Accumulated step count" },
    { MSG_CODE_BLOOD_OXYGEN,    "BLOOD_OXYGEN",     "Blood oxygen saturation (SpO2)" },
    { MSG_CODE_SLEEP_STATE,     "SLEEP_STATE",      "Sleep state classification" },
    { MSG_CODE_ACCEL_DATA,      "ACCEL_DATA",       "Accelerometer raw data" },
    { MSG_CODE_GYRO_DATA,       "GYRO_DATA",        "Gyroscope raw data" },
    { MSG_CODE_CALORIES,        "CALORIES",         "Calories burned (kcal)" },
    { MSG_CODE_DISTANCE,        "DISTANCE",         "Distance traveled (meters)" },
    { MSG_CODE_BP_SYSTOLIC,     "BP_SYSTOLIC",      "Blood pressure systolic (mmHg)" },
    { MSG_CODE_BP_DIASTOLIC,    "BP_DIASTOLIC",     "Blood pressure diastolic (mmHg)" },
    { MSG_CODE_ECG_SAMPLE,      "ECG_SAMPLE",       "ECG waveform sample" },
    { MSG_CODE_FALL_DETECT,     "FALL_DETECT",      "Fall detection event" },
    { MSG_CODE_CHARGER_STATE,   "CHARGER_STATE",    "Charger connection state" },
    { MSG_CODE_ALARM_EVENT,     "ALARM_EVENT",      "User-configured alarm" },
    { MSG_CODE_FIRMWARE_VERSION,"FW_VERSION",       "Firmware version identifier" },
    { MSG_CODE_SENSOR_STATE,    "SENSOR_STATE",     "Sensor operational state" },
    { MSG_CODE_USER_ACTIVITY,   "USER_ACTIVITY",    "User activity classification" },
    { MSG_CODE_PPG_RAW,         "PPG_RAW",          "PPG raw data stream" },
    { MSG_CODE_SKIN_TEMP,       "SKIN_TEMP",        "Skin temperature (Celsius)" },
    { MSG_CODE_HUMIDITY,        "HUMIDITY",         "Ambient humidity (%)" },
    { MSG_CODE_UV_INDEX,        "UV_INDEX",         "Ultraviolet radiation index" },
    { MSG_CODE_AIR_QUALITY,     "AIR_QUALITY",      "Air quality index" },
    { MSG_CODE_MOTION_MODE,     "MOTION_MODE",      "Motion mode recognition" },
    { MSG_CODE_WATER_INTAKE,    "WATER_INTAKE",     "Water intake tracking (ml)" },
    { MSG_CODE_STRESS_LEVEL,    "STRESS_LEVEL",     "Stress level assessment" },
    { MSG_CODE_RESP_RATE,       "RESP_RATE",        "Respiration rate (breaths/min)" },
    { MSG_CODE_HRV,             "HRV",              "Heart rate variability (ms)" },
};

const int msg_table_count = sizeof(msg_table) / sizeof(msg_table[0]);

const char *translate_message(uint32_t code)
{
    int i;
    for (i = 0; i < msg_table_count; i++) {
        if (msg_table[i].code == code)
            return msg_table[i].name;
    }
    return "UNKNOWN";
}

const MessageTranslation *translate_message_entry(uint32_t code)
{
    int i;
    for (i = 0; i < msg_table_count; i++) {
        if (msg_table[i].code == code)
            return &msg_table[i];
    }
    return NULL;
}
