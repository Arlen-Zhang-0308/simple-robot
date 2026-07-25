#include "motion_watchdog.h"

#include "app_config.h"

static uint32_t elapsed_ms;
static bool active;

void motion_watchdog_init(void)
{
    elapsed_ms = 0U;
    active = false;
}

void motion_watchdog_refresh(void)
{
    elapsed_ms = 0U;
    active = true;
}

void motion_watchdog_cancel(void)
{
    elapsed_ms = 0U;
    active = false;
}

bool motion_watchdog_tick(uint32_t tick_ms)
{
    if (!active) {
        return false;
    }

    if (tick_ms >= (APP_MOTION_COMMAND_TIMEOUT_MS - elapsed_ms)) {
        elapsed_ms = APP_MOTION_COMMAND_TIMEOUT_MS;
        active = false;
        return true;
    }

    elapsed_ms += tick_ms;
    return false;
}

uint32_t motion_watchdog_elapsed_ms(void)
{
    return elapsed_ms;
}
