#include "protocol.h"

#include <string.h>

#include "crc8.h"

static void protocol_reset(ProtocolParser *parser)
{
    parser->state = PROTOCOL_WAIT_HEADER_AA;
    parser->length = 0U;
    parser->body_index = 0U;
}

void protocol_parser_init(ProtocolParser *parser)
{
    if (parser != NULL) {
        protocol_reset(parser);
    }
}

ProtocolParseResult protocol_parser_consume(
    ProtocolParser *parser,
    uint8_t byte,
    ProtocolFrame *frame)
{
    if ((parser == NULL) || (frame == NULL)) {
        return PROTOCOL_PARSE_INCOMPLETE;
    }

    switch (parser->state) {
    case PROTOCOL_WAIT_HEADER_AA:
        if (byte == PROTOCOL_HEADER_H) {
            parser->state = PROTOCOL_WAIT_HEADER_55;
        }
        break;

    case PROTOCOL_WAIT_HEADER_55:
        if (byte == PROTOCOL_HEADER_L) {
            parser->state = PROTOCOL_WAIT_LENGTH;
        } else if (byte != PROTOCOL_HEADER_H) {
            parser->state = PROTOCOL_WAIT_HEADER_AA;
        }
        break;

    case PROTOCOL_WAIT_LENGTH:
        if ((byte < 1U) || (byte > (1U + PROTOCOL_MAX_PAYLOAD))) {
            protocol_reset(parser);
            return PROTOCOL_PARSE_BAD_LENGTH;
        }
        parser->length = byte;
        parser->body_index = 0U;
        parser->state = PROTOCOL_WAIT_BODY;
        break;

    case PROTOCOL_WAIT_BODY:
        parser->body[parser->body_index++] = byte;
        if (parser->body_index == parser->length) {
            parser->state = PROTOCOL_WAIT_CHECKSUM;
        }
        break;

    case PROTOCOL_WAIT_CHECKSUM: {
        uint8_t crc_data[2U + PROTOCOL_MAX_PAYLOAD];
        uint8_t expected;

        crc_data[0] = parser->length;
        memcpy(&crc_data[1], parser->body, parser->length);
        expected = crc8_compute(crc_data, (size_t)parser->length + 1U);
        if (expected != byte) {
            protocol_reset(parser);
            return PROTOCOL_PARSE_BAD_CRC;
        }

        frame->command = parser->body[0];
        frame->payload_length = (uint8_t)(parser->length - 1U);
        if (frame->payload_length != 0U) {
            memcpy(frame->payload, &parser->body[1], frame->payload_length);
        }
        protocol_reset(parser);
        return PROTOCOL_PARSE_FRAME_READY;
    }

    default:
        protocol_reset(parser);
        break;
    }

    return PROTOCOL_PARSE_INCOMPLETE;
}

size_t protocol_encode(
    uint8_t command,
    const uint8_t *payload,
    uint8_t payload_length,
    uint8_t *output,
    size_t output_capacity)
{
    size_t frame_length = (size_t)payload_length + 5U;

    if ((output == NULL) ||
        (payload_length > PROTOCOL_MAX_PAYLOAD) ||
        ((payload == NULL) && (payload_length != 0U)) ||
        (output_capacity < frame_length)) {
        return 0U;
    }

    output[0] = PROTOCOL_HEADER_H;
    output[1] = PROTOCOL_HEADER_L;
    output[2] = (uint8_t)(payload_length + 1U);
    output[3] = command;
    if (payload_length != 0U) {
        memcpy(&output[4], payload, payload_length);
    }
    output[frame_length - 1U] = crc8_compute(&output[2], (size_t)payload_length + 2U);
    return frame_length;
}
