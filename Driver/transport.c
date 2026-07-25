#include "transport.h"

#include "module_config.h"

bool transport_is_enabled(TransportId transport)
{
    switch (transport) {
    case TRANSPORT_UART:
        return APP_ENABLE_TRANSPORT_UART != 0;
    case TRANSPORT_NRF24L01:
        return APP_ENABLE_TRANSPORT_NRF24L01 != 0;
    case TRANSPORT_BLUETOOTH:
        return APP_ENABLE_TRANSPORT_BLUETOOTH != 0;
    case TRANSPORT_WIFI:
        return APP_ENABLE_TRANSPORT_WIFI != 0;
    default:
        return false;
    }
}

TransportEncryption transport_encryption(TransportId transport)
{
    switch (transport) {
    case TRANSPORT_UART:
        return (TransportEncryption)APP_TRANSPORT_UART_ENCRYPTION;
    case TRANSPORT_NRF24L01:
        return (TransportEncryption)APP_TRANSPORT_NRF24L01_ENCRYPTION;
    case TRANSPORT_BLUETOOTH:
        return (TransportEncryption)APP_TRANSPORT_BLUETOOTH_ENCRYPTION;
    case TRANSPORT_WIFI:
        return (TransportEncryption)APP_TRANSPORT_WIFI_ENCRYPTION;
    default:
        return TRANSPORT_ENCRYPTION_NONE;
    }
}
