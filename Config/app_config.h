#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "module_config.h"
#include "protocol_config.h"
#include "rtos_config.h"

#define APP_FIRMWARE_NAME          "simple-robot"
#define APP_FIRMWARE_VERSION_MAJOR 0U
#define APP_FIRMWARE_VERSION_MINOR 1U
#define APP_FIRMWARE_VERSION_PATCH 0U

#define APP_MOTION_COMMAND_TIMEOUT_MS 500U

#define APP_DISPLAY_ANIMATION_FRAME_MS  50U
#define APP_DISPLAY_ANIMATION_FRAMES   240U
#define APP_DISPLAY_IDLE_TIMEOUT_MS   3000U

#if (APP_MOTION_COMMAND_TIMEOUT_MS % RTOS_MOTION_TICK_MS) != 0U
#error "Motion command timeout must align with MotionTask period"
#endif

#if APP_DISPLAY_ANIMATION_FRAME_MS < RTOS_DISPLAY_PERIOD_MS
#error "Display animation frame period must not be shorter than DisplayTask period"
#endif

#if APP_DISPLAY_ANIMATION_FRAMES != 240U
#error "Mechanical morph animation requires six 40-frame phases"
#endif

#define APP_DEBUG_ENABLE  1
#define APP_LOG_ENABLE    1
#define APP_ASSERT_ENABLE 1

#endif
