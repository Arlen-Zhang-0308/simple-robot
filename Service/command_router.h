#ifndef COMMAND_ROUTER_H
#define COMMAND_ROUTER_H

#include <stdbool.h>
#include <stdint.h>

#include "protocol.h"

typedef struct {
    uint8_t command;
    uint8_t payload_length;
    uint8_t payload[PROTOCOL_MAX_PAYLOAD];
} CommandResponse;

bool command_router_handle(const ProtocolFrame *request, CommandResponse *response);

#endif
