#ifndef MOTION_WATCHDOG_H
#define MOTION_WATCHDOG_H

#include <stdbool.h>
#include <stdint.h>

void motion_watchdog_init(void);
void motion_watchdog_refresh(void);
void motion_watchdog_cancel(void);
bool motion_watchdog_tick(uint32_t elapsed_ms);
uint32_t motion_watchdog_elapsed_ms(void);

#endif
