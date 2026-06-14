#ifndef CRC8_SMBUS_H
#define CRC8_SMBUS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t crc8_smbus_update(uint8_t crc, const void *data, size_t len);
uint8_t crc8_smbus_compute(const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif
