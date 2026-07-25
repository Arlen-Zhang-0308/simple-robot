#include "communication_system.h"

#include "command_router.h"
#include "protocol.h"
#include "ring_buffer.h"
#include "robot_state.h"

#define TRANSPORT_RX_STORAGE_SIZE 128U

typedef struct {
    uint8_t storage[TRANSPORT_RX_STORAGE_SIZE];
    RingBuffer rx_buffer;
    ProtocolParser parser;
} CommunicationChannel;

static CommunicationChannel channels[TRANSPORT_COUNT];
static TransportId next_transport;

static size_t encode_nack(
    uint8_t reason,
    uint8_t *response_frame,
    size_t response_capacity)
{
    robot_state_set_error(reason);
    return protocol_encode(
        CMD_NACK,
        &reason,
        1U,
        response_frame,
        response_capacity);
}

void communication_system_init(void)
{
    TransportId transport;

    for (transport = TRANSPORT_UART;
         transport < TRANSPORT_COUNT;
         transport = (TransportId)(transport + 1)) {
        (void)ring_buffer_init(
            &channels[transport].rx_buffer,
            channels[transport].storage,
            sizeof(channels[transport].storage));
        protocol_parser_init(&channels[transport].parser);
    }
    next_transport = TRANSPORT_UART;
}

bool communication_system_receive_byte(TransportId transport, uint8_t byte)
{
    if ((transport >= TRANSPORT_COUNT) || !transport_is_enabled(transport)) {
        return false;
    }
    return ring_buffer_push(&channels[transport].rx_buffer, byte);
}

size_t communication_system_process(
    TransportId *response_transport,
    uint8_t *response_frame,
    size_t response_capacity)
{
    TransportId checked;

    if ((response_transport == NULL) ||
        (response_frame == NULL) ||
        (response_capacity == 0U)) {
        return 0U;
    }

    for (checked = TRANSPORT_UART;
         checked < TRANSPORT_COUNT;
         checked = (TransportId)(checked + 1)) {
        TransportId transport = (TransportId)(
            (next_transport + checked) % TRANSPORT_COUNT);
        CommunicationChannel *channel = &channels[transport];
        uint8_t byte;

        if (!transport_is_enabled(transport)) {
            continue;
        }

        while (ring_buffer_pop(&channel->rx_buffer, &byte)) {
            ProtocolFrame request;
            ProtocolParseResult result = protocol_parser_consume(
                &channel->parser,
                byte,
                &request);

            if (result == PROTOCOL_PARSE_FRAME_READY) {
                CommandResponse response;

                if (!command_router_handle(&request, &response)) {
                    return 0U;
                }
                *response_transport = transport;
                next_transport = (TransportId)(
                    (transport + 1) % TRANSPORT_COUNT);
                return protocol_encode(
                    response.command,
                    response.payload,
                    response.payload_length,
                    response_frame,
                    response_capacity);
            }

            if ((result == PROTOCOL_PARSE_BAD_CRC) ||
                (result == PROTOCOL_PARSE_BAD_LENGTH)) {
                uint8_t reason = (result == PROTOCOL_PARSE_BAD_CRC)
                    ? PROTO_ERR_BAD_CRC
                    : PROTO_ERR_BAD_LEN;

                *response_transport = transport;
                next_transport = (TransportId)(
                    (transport + 1) % TRANSPORT_COUNT);
                return encode_nack(
                    reason,
                    response_frame,
                    response_capacity);
            }
        }
    }

    return 0U;
}
