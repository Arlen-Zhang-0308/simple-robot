#ifndef COMMUNICATION_SYSTEM_H
#define COMMUNICATION_SYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "transport.h"

void communication_system_init(void);
bool communication_system_receive_byte(TransportId transport, uint8_t byte);
size_t communication_system_process(
    TransportId *response_transport,
    uint8_t *response_frame,
    size_t response_capacity);

#endif
