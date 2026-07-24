#include "crc8.h"

#include "protocol_config.h"

uint8_t crc8_compute(const uint8_t *data, size_t length)
{
    uint8_t crc = PROTOCOL_CRC8_INIT;
    size_t index;

    if ((data == NULL) && (length != 0U)) {
        return crc;
    }

    for (index = 0U; index < length; ++index) {
        uint8_t bit;
        crc ^= data[index];
        for (bit = 0U; bit < 8U; ++bit) {
            crc = ((crc & 0x80U) != 0U)
                ? (uint8_t)((crc << 1U) ^ PROTOCOL_CRC8_POLY)
                : (uint8_t)(crc << 1U);
        }
    }

    return crc;
}
