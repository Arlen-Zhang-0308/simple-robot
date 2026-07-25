#include "motor_driver.h"

#include "main.h"
#include "module_config.h"
#include "pin_config.h"

extern TIM_HandleTypeDef htim4;

static MotorDriverOutput output;
static bool initialized;

static uint16_t speed_to_compare(uint8_t speed)
{
    uint8_t limited_speed = (speed > 100U) ? 100U : speed;
    return (uint16_t)(((uint32_t)limited_speed * MOTOR_PWM_PERIOD_COUNTS) / 100U);
}

static void write_output(void)
{
    __HAL_TIM_SET_COMPARE(&htim4, MOTOR_LEFT_IN1_CHANNEL, output.left_in1);
    __HAL_TIM_SET_COMPARE(&htim4, MOTOR_LEFT_IN2_CHANNEL, output.left_in2);
    __HAL_TIM_SET_COMPARE(&htim4, MOTOR_RIGHT_IN1_CHANNEL, output.right_in1);
    __HAL_TIM_SET_COMPARE(&htim4, MOTOR_RIGHT_IN2_CHANNEL, output.right_in2);
}

static void set_left_forward(uint16_t compare)
{
    output.left_in1 = compare;
    output.left_in2 = 0U;
}

static void set_left_backward(uint16_t compare)
{
    output.left_in1 = 0U;
    output.left_in2 = compare;
}

static void set_right_forward(uint16_t compare)
{
    output.right_in1 = compare;
    output.right_in2 = 0U;
}

static void set_right_backward(uint16_t compare)
{
    output.right_in1 = 0U;
    output.right_in2 = compare;
}

bool motor_driver_init(void)
{
    if (!APP_ENABLE_MOTOR || (APP_MOTION_IMPL != APP_MOTION_IMPL_REAL_PWM)) {
        return false;
    }

    motor_driver_stop();
    if ((HAL_TIM_PWM_Start(&htim4, MOTOR_LEFT_IN1_CHANNEL) != HAL_OK) ||
        (HAL_TIM_PWM_Start(&htim4, MOTOR_LEFT_IN2_CHANNEL) != HAL_OK) ||
        (HAL_TIM_PWM_Start(&htim4, MOTOR_RIGHT_IN1_CHANNEL) != HAL_OK) ||
        (HAL_TIM_PWM_Start(&htim4, MOTOR_RIGHT_IN2_CHANNEL) != HAL_OK)) {
        motor_driver_stop();
        return false;
    }

    initialized = true;
    write_output();
    return true;
}

void motor_driver_apply(MotionState motion, uint8_t speed)
{
    uint16_t compare = speed_to_compare(speed);

    switch (motion) {
    case MOTION_STATE_FORWARD:
        set_left_forward(compare);
        set_right_forward(compare);
        break;
    case MOTION_STATE_BACKWARD:
        set_left_backward(compare);
        set_right_backward(compare);
        break;
    case MOTION_STATE_TURN_LEFT:
        set_left_backward(compare);
        set_right_forward(compare);
        break;
    case MOTION_STATE_TURN_RIGHT:
        set_left_forward(compare);
        set_right_backward(compare);
        break;
    case MOTION_STATE_STOPPED:
    case MOTION_STATE_EMERGENCY_STOP:
    default:
        motor_driver_stop();
        return;
    }

    if (initialized) {
        write_output();
    }
}

void motor_driver_stop(void)
{
    output.left_in1 = 0U;
    output.left_in2 = 0U;
    output.right_in1 = 0U;
    output.right_in2 = 0U;
    write_output();
}

MotorDriverOutput motor_driver_output(void)
{
    return output;
}
