#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include "soc_init.h"
#include "uart_printf.h"
#include "vpi_error.h"
#include "message_defs.h"

/* Exception handler registration from bsp library */
extern int system_exception_register(int exc_code, void *handler);

/* DSP software emulation for illegal instruction (mcause=2) */
extern int dsp_emu_illegal_handler(uintptr_t mcause, uint32_t *ctx);

static void busy_delay(int ms)
{
    volatile uint32_t count = 0;
    uint32_t target = ms * 3200;
    while (count < target) {
        count++;
    }
}

int main(void)
{
    int ret;

    ret = soc_init();
    ret = vsd_to_vpi(ret);
    if (ret != VPI_SUCCESS) {
        uart_printf("soc init error %d\r\n", ret);
        while (1) ;
    }
    uart_printf("soc init done\r\n");

    system_exception_register(2, (void *)dsp_emu_illegal_handler);
    uart_printf("DSP emulation enabled\r\n");

    uart_printf("=== Message Translation Demo ===\r\n");

    static const uint32_t codes[] = {
        MSG_CODE_HEARTBEAT,
        MSG_CODE_TEMPERATURE,
        MSG_CODE_BATTERY_LEVEL,
        MSG_CODE_HEART_RATE,
        MSG_CODE_STEP_COUNT,
        MSG_CODE_BLOOD_OXYGEN,
        MSG_CODE_SLEEP_STATE,
        MSG_CODE_CALORIES,
        MSG_CODE_DISTANCE,
        MSG_CODE_BP_SYSTOLIC,
        MSG_CODE_BP_DIASTOLIC,
        MSG_CODE_FALL_DETECT,
        MSG_CODE_STRESS_LEVEL,
        MSG_CODE_HRV,
        MSG_CODE_ACCEL_DATA,
        MSG_CODE_GYRO_DATA,
        MSG_CODE_CHARGER_STATE,
        MSG_CODE_FIRMWARE_VERSION,
        MSG_CODE_SENSOR_STATE,
        MSG_CODE_USER_ACTIVITY,
        MSG_CODE_PPG_RAW,
        MSG_CODE_SKIN_TEMP,
        MSG_CODE_HUMIDITY,
        MSG_CODE_UV_INDEX,
        MSG_CODE_AIR_QUALITY,
        MSG_CODE_MOTION_MODE,
        MSG_CODE_WATER_INTAKE,
        MSG_CODE_RESP_RATE,
        MSG_CODE_ECG_SAMPLE,
        MSG_CODE_ALARM_EVENT,
    };
    int msg_count = sizeof(codes) / sizeof(codes[0]);

    int count = 0;
    while (count < 5) {
        for (int i = 0; i < msg_count; i++) {
            uint32_t code = codes[i];
            const char *name = translate_message(code);
            uart_printf("[%02d] 0x%04" PRIx32 " -> %s\r\n", count, code, name);
            busy_delay(500);
        }
        count++;
    }

    uart_printf("=== Message Translation Demo Complete ===\r\n");
    while (1) ;
}
