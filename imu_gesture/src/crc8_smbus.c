#include "crc8_smbus.h"

#define CRC8_SMBUS_POLY 0x07u

uint8_t crc8_smbus_update(uint8_t crc, const void *data, size_t len)
{
    const uint8_t *bytes = (const uint8_t *)data;

    while (len-- > 0u) {
        uint8_t bit;

        crc ^= *bytes++;
        for (bit = 0u; bit < 8u; bit++) {
            if ((crc & 0x80u) != 0u) {
                crc = (uint8_t)((crc << 1u) ^ CRC8_SMBUS_POLY);
            } else {
                crc = (uint8_t)(crc << 1u);
            }
        }
    }

    return crc;
}

uint8_t crc8_smbus_compute(const void *data, size_t len)
{
    return crc8_smbus_update(0u, data, len);
}
