#include "app_tasks.h"

#include "app_config.h"
#include "command_router.h"
#include "display_stub.h"
#include "protocol.h"
#include "ring_buffer.h"
#include "robot_state.h"

#define UART_RX_STORAGE_SIZE 128U
#define SIMULATED_INPUT_MV   5000U
#define POWER_LOW_MV         4500U

static uint8_t uart_rx_storage[UART_RX_STORAGE_SIZE];
static RingBuffer uart_rx_buffer;
static ProtocolParser protocol_parser;

void app_tasks_init(void)
{
    (void)ring_buffer_init(&uart_rx_buffer, uart_rx_storage, sizeof(uart_rx_storage));
    protocol_parser_init(&protocol_parser);
    robot_state_init();
    display_stub_init();
}

bool comm_task_rx_byte(uint8_t byte)
{
    return ring_buffer_push(&uart_rx_buffer, byte);
}

size_t comm_task_process(uint8_t *tx_frame, size_t tx_capacity)
{
    uint8_t byte;
    ProtocolFrame request;
    CommandResponse response;

    while (ring_buffer_pop(&uart_rx_buffer, &byte)) {
        ProtocolParseResult result = protocol_parser_consume(&protocol_parser, byte, &request);
        if (result == PROTOCOL_PARSE_FRAME_READY) {
            if (!command_router_handle(&request, &response)) {
                return 0U;
            }
            return protocol_encode(response.command, response.payload,
                                   response.payload_length, tx_frame, tx_capacity);
        }
        if ((result == PROTOCOL_PARSE_BAD_CRC) ||
            (result == PROTOCOL_PARSE_BAD_LENGTH)) {
            robot_state_set_error((result == PROTOCOL_PARSE_BAD_CRC)
                                      ? PROTO_ERR_BAD_CRC
                                      : PROTO_ERR_BAD_LEN);
        }
    }

    return 0U;
}

void motion_task_step(void)
{
    RobotState state = robot_state_snapshot();
    if (state.sensor.low_power && (state.motion != MOTION_STATE_EMERGENCY_STOP)) {
        robot_state_set_motion(MOTION_STATE_STOPPED, 0U);
    }
}

void display_task_step(void)
{
    RobotState state = robot_state_snapshot();
    display_stub_render(&state);
}

void sensor_task_step(void)
{
#if APP_POWER_SENSOR_IMPL == APP_SENSOR_IMPL_SIMULATED
    robot_state_set_sensor(SIMULATED_INPUT_MV, SIMULATED_INPUT_MV < POWER_LOW_MV);
#endif
}

void comm_task(void *argument)
{
    (void)argument;
    for (;;) {
        /* CubeMX/FreeRTOS port: block on UART RX notification, then call comm_task_process(). */
    }
}

void motion_task(void *argument)
{
    (void)argument;
    for (;;) {
        /* CubeMX/FreeRTOS port: wait for motion command or periodic safety tick. */
    }
}

void display_task(void *argument)
{
    (void)argument;
    for (;;) {
        /* CubeMX/FreeRTOS port: call display_task_step() every RTOS_DISPLAY_PERIOD_MS. */
    }
}

void sensor_task(void *argument)
{
    (void)argument;
    for (;;) {
        /* CubeMX/FreeRTOS port: call sensor_task_step() every RTOS_SENSOR_PERIOD_MS. */
    }
}
