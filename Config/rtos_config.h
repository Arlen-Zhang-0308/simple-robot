#ifndef RTOS_CONFIG_H
#define RTOS_CONFIG_H

#define RTOS_PRIO_DISPLAY          1U
#define RTOS_PRIO_SENSOR           2U
#define RTOS_PRIO_MOTION           3U
#define RTOS_PRIO_COMM             3U

#define RTOS_STACK_COMM            256U
#define RTOS_STACK_MOTION          256U
#define RTOS_STACK_SENSOR          192U
#define RTOS_STACK_DISPLAY         384U

#define RTOS_QUEUE_COMMAND_LEN     8U
#define RTOS_QUEUE_DISPLAY_CMD_LEN 4U

#define RTOS_SENSOR_PERIOD_MS      100U
#define RTOS_DISPLAY_PERIOD_MS     33U
#define RTOS_MOTION_TICK_MS        20U

#endif
