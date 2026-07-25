#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "robot_state.h"

typedef struct {
    uint16_t left_in1;
    uint16_t left_in2;
    uint16_t right_in1;
    uint16_t right_in2;
} MotorDriverOutput;

bool motor_driver_init(void);
void motor_driver_apply(MotionState motion, uint8_t speed);
void motor_driver_stop(void);
MotorDriverOutput motor_driver_output(void);

#endif
