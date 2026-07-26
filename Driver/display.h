#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>

#include "robot_state.h"

void display_init(void);
void display_render(const RobotState *state);
bool display_is_ready(void);

#endif
