#include "app_tasks.h"

#include "app_config.h"
#include "communication_system.h"
#include "display_stub.h"
#include "motor_driver.h"
#include "motion_watchdog.h"
#include "robot_state.h"

#define SIMULATED_INPUT_MV   5000U
#define POWER_LOW_MV         4500U

void app_tasks_init(void)
{
    communication_system_init();
    motion_watchdog_init();
    robot_state_init();
    display_stub_init();
    (void)motor_driver_init();
}

bool comm_task_rx_byte(uint8_t byte)
{
    return comm_task_transport_rx_byte(TRANSPORT_UART, byte);
}

size_t comm_task_process(uint8_t *tx_frame, size_t tx_capacity)
{
    TransportId response_transport;

    return comm_task_process_transport(
        &response_transport,
        tx_frame,
        tx_capacity);
}

bool comm_task_transport_rx_byte(TransportId transport, uint8_t byte)
{
    return communication_system_receive_byte(transport, byte);
}

size_t comm_task_process_transport(
    TransportId *response_transport,
    uint8_t *tx_frame,
    size_t tx_capacity)
{
    return communication_system_process(
        response_transport,
        tx_frame,
        tx_capacity);
}

void motion_task_step(void)
{
    RobotState state = robot_state_snapshot();
    if (motion_watchdog_tick(RTOS_MOTION_TICK_MS)) {
        robot_state_set_motion(MOTION_STATE_STOPPED, 0U);
        state = robot_state_snapshot();
    }
    if (state.sensor.low_power && (state.motion != MOTION_STATE_EMERGENCY_STOP)) {
        motion_watchdog_cancel();
        robot_state_set_motion(MOTION_STATE_STOPPED, 0U);
        state = robot_state_snapshot();
    }
    motor_driver_apply(state.motion, state.speed);
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
