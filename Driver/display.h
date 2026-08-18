#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "robot_state.h"

void display_init(void);
void display_render(const RobotState *state);
void display_render_animation(void);
bool display_is_ready(void);

#endif
