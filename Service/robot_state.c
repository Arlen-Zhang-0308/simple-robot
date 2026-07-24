#include "robot_state.h"

#include <string.h>

static RobotState state;

void robot_state_init(void)
{
    memset(&state, 0, sizeof(state));
    state.motion = MOTION_STATE_STOPPED;
}

RobotState robot_state_snapshot(void)
{
    return state;
}

void robot_state_set_motion(MotionState motion, uint8_t speed)
{
    state.motion = motion;
    state.speed = speed;
}

void robot_state_set_sensor(uint16_t input_voltage_mv, bool low_power)
{
    state.sensor.input_voltage_mv = input_voltage_mv;
    state.sensor.low_power = low_power;
}

void robot_state_set_display_page(uint8_t page)
{
    state.display_page = page;
}

void robot_state_set_error(uint8_t error)
{
    state.last_error = error;
}
