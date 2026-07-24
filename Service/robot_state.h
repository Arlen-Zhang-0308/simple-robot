#ifndef ROBOT_STATE_H
#define ROBOT_STATE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MOTION_STATE_STOPPED = 0,
    MOTION_STATE_FORWARD,
    MOTION_STATE_BACKWARD,
    MOTION_STATE_TURN_LEFT,
    MOTION_STATE_TURN_RIGHT,
    MOTION_STATE_EMERGENCY_STOP
} MotionState;

typedef struct {
    uint16_t input_voltage_mv;
    bool low_power;
} SensorState;

typedef struct {
    MotionState motion;
    uint8_t speed;
    uint8_t display_page;
    uint8_t last_error;
    SensorState sensor;
} RobotState;

void robot_state_init(void);
RobotState robot_state_snapshot(void);
void robot_state_set_motion(MotionState motion, uint8_t speed);
void robot_state_set_sensor(uint16_t input_voltage_mv, bool low_power);
void robot_state_set_display_page(uint8_t page);
void robot_state_set_error(uint8_t error);

#endif
