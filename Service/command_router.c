#include "command_router.h"

#include "module_config.h"
#include "robot_state.h"

static void set_ack(CommandResponse *response, uint8_t original_command)
{
    response->command = CMD_ACK;
    response->payload_length = 1U;
    response->payload[0] = original_command;
}

static void set_nack(CommandResponse *response, uint8_t reason)
{
    response->command = CMD_NACK;
    response->payload_length = 1U;
    response->payload[0] = reason;
    robot_state_set_error(reason);
}

static bool require_speed(const ProtocolFrame *request, CommandResponse *response)
{
    if ((request->payload_length != 1U) || (request->payload[0] > 100U)) {
        set_nack(response, PROTO_ERR_BAD_PAYLOAD);
        return false;
    }
    return true;
}

bool command_router_handle(const ProtocolFrame *request, CommandResponse *response)
{
    RobotState state;

    if ((request == NULL) || (response == NULL)) {
        return false;
    }

    switch (request->command) {
    case CMD_PING:
        if (request->payload_length != 0U) {
            set_nack(response, PROTO_ERR_BAD_PAYLOAD);
        } else {
            set_ack(response, CMD_PING);
        }
        break;

    case CMD_GET_STATUS:
        if (request->payload_length != 0U) {
            set_nack(response, PROTO_ERR_BAD_PAYLOAD);
            break;
        }
        state = robot_state_snapshot();
        response->command = CMD_STATUS;
        response->payload_length = 7U;
        response->payload[0] = (uint8_t)state.motion;
        response->payload[1] = state.speed;
        response->payload[2] = state.display_page;
        response->payload[3] = state.last_error;
        response->payload[4] = (uint8_t)(state.sensor.input_voltage_mv >> 8U);
        response->payload[5] = (uint8_t)state.sensor.input_voltage_mv;
        response->payload[6] = state.sensor.low_power ? 1U : 0U;
        break;

    case CMD_GET_POWER:
        if (request->payload_length != 0U) {
            set_nack(response, PROTO_ERR_BAD_PAYLOAD);
            break;
        }
        state = robot_state_snapshot();
        response->command = CMD_POWER;
        response->payload_length = 3U;
        response->payload[0] = (uint8_t)(state.sensor.input_voltage_mv >> 8U);
        response->payload[1] = (uint8_t)state.sensor.input_voltage_mv;
        response->payload[2] = state.sensor.low_power ? 1U : 0U;
        break;

    case CMD_MOVE_STOP:
        if (request->payload_length != 0U) {
            set_nack(response, PROTO_ERR_BAD_PAYLOAD);
            break;
        }
        robot_state_set_motion(MOTION_STATE_STOPPED, 0U);
        set_ack(response, request->command);
        break;

    case CMD_MOVE_FORWARD:
    case CMD_MOVE_BACKWARD:
    case CMD_TURN_LEFT:
    case CMD_TURN_RIGHT:
        if (!require_speed(request, response)) {
            break;
        }
        state = robot_state_snapshot();
        if (state.sensor.low_power) {
            set_nack(response, PROTO_ERR_LOW_POWER);
            break;
        }
        robot_state_set_motion(
            (request->command == CMD_MOVE_FORWARD) ? MOTION_STATE_FORWARD :
            (request->command == CMD_MOVE_BACKWARD) ? MOTION_STATE_BACKWARD :
            (request->command == CMD_TURN_LEFT) ? MOTION_STATE_TURN_LEFT :
                                                  MOTION_STATE_TURN_RIGHT,
            request->payload[0]);
        set_ack(response, request->command);
        break;

    case CMD_EMERGENCY_STOP:
        if (request->payload_length != 0U) {
            set_nack(response, PROTO_ERR_BAD_PAYLOAD);
            break;
        }
        robot_state_set_motion(MOTION_STATE_EMERGENCY_STOP, 0U);
        set_ack(response, request->command);
        break;

    case CMD_DISPLAY_PAGE:
        if (request->payload_length != 1U) {
            set_nack(response, PROTO_ERR_BAD_PAYLOAD);
            break;
        }
        robot_state_set_display_page(request->payload[0]);
        set_ack(response, request->command);
        break;

    case CMD_SERVO_SET:
    case CMD_SERVO_HOME:
#if APP_ENABLE_SERVO
        set_ack(response, request->command);
#else
        set_nack(response, PROTO_ERR_DISABLED_MODULE);
#endif
        break;

    default:
        set_nack(response, PROTO_ERR_UNKNOWN_CMD);
        break;
    }

    return true;
}
