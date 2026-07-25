#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TRANSPORT_UART = 0,
    TRANSPORT_NRF24L01,
    TRANSPORT_BLUETOOTH,
    TRANSPORT_WIFI,
    TRANSPORT_COUNT
} TransportId;

typedef enum {
    TRANSPORT_ENCRYPTION_NONE = 0,
    TRANSPORT_ENCRYPTION_LINK,
    TRANSPORT_ENCRYPTION_APPLICATION
} TransportEncryption;

bool transport_is_enabled(TransportId transport);
TransportEncryption transport_encryption(TransportId transport);

#endif
