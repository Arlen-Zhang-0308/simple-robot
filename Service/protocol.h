#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#include "protocol_config.h"

typedef struct {
    uint8_t command;
    uint8_t payload_length;
    uint8_t payload[PROTOCOL_MAX_PAYLOAD];
} ProtocolFrame;

typedef enum {
    PROTOCOL_PARSE_INCOMPLETE = 0,
    PROTOCOL_PARSE_FRAME_READY,
    PROTOCOL_PARSE_BAD_LENGTH,
    PROTOCOL_PARSE_BAD_CRC
} ProtocolParseResult;

typedef enum {
    PROTOCOL_WAIT_HEADER_AA = 0,
    PROTOCOL_WAIT_HEADER_55,
    PROTOCOL_WAIT_LENGTH,
    PROTOCOL_WAIT_BODY,
    PROTOCOL_WAIT_CHECKSUM
} ProtocolParserState;

typedef struct {
    ProtocolParserState state;
    uint8_t length;
    uint8_t body[1U + PROTOCOL_MAX_PAYLOAD];
    uint8_t body_index;
} ProtocolParser;

void protocol_parser_init(ProtocolParser *parser);
ProtocolParseResult protocol_parser_consume(
    ProtocolParser *parser,
    uint8_t byte,
    ProtocolFrame *frame);
size_t protocol_encode(
    uint8_t command,
    const uint8_t *payload,
    uint8_t payload_length,
    uint8_t *output,
    size_t output_capacity);

#endif
