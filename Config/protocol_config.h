#ifndef PROTOCOL_CONFIG_H
#define PROTOCOL_CONFIG_H

#define PROTOCOL_HEADER_H        0xAAU
#define PROTOCOL_HEADER_L        0x55U
#define PROTOCOL_MAX_PAYLOAD     32U
#define PROTOCOL_MAX_FRAME       (2U + 1U + 1U + PROTOCOL_MAX_PAYLOAD + 1U)
#define PROTOCOL_CRC8_POLY       0x07U
#define PROTOCOL_CRC8_INIT       0x00U

#define CMD_PING                 0x01U
#define CMD_GET_STATUS           0x02U
#define CMD_MOVE_STOP            0x10U
#define CMD_MOVE_FORWARD         0x11U
#define CMD_MOVE_BACKWARD        0x12U
#define CMD_TURN_LEFT            0x13U
#define CMD_TURN_RIGHT           0x14U
#define CMD_SERVO_SET            0x20U
#define CMD_SERVO_HOME           0x21U
#define CMD_GET_POWER            0x30U
#define CMD_DISPLAY_PAGE         0x40U
#define CMD_EMERGENCY_STOP       0x7FU

#define CMD_ACK                  0x80U
#define CMD_NACK                 0x81U
#define CMD_STATUS               0x82U
#define CMD_POWER                0x83U

#define PROTO_ERR_NONE            0x00U
#define PROTO_ERR_BAD_CRC         0x01U
#define PROTO_ERR_BAD_LEN         0x02U
#define PROTO_ERR_UNKNOWN_CMD     0x03U
#define PROTO_ERR_BAD_PAYLOAD     0x04U
#define PROTO_ERR_BUSY            0x05U
#define PROTO_ERR_DISABLED_MODULE 0x06U
#define PROTO_ERR_LOW_POWER       0x07U

#endif
