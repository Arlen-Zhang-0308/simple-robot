#ifndef DISPLAY_STUB_H
#define DISPLAY_STUB_H

#include "robot_state.h"

void display_stub_init(void);
void display_stub_render(const RobotState *state);
RobotState display_stub_last_frame(void);

#endif
