#include "display_stub.h"

#include <string.h>

static RobotState last_frame;

void display_stub_init(void)
{
    memset(&last_frame, 0, sizeof(last_frame));
}

void display_stub_render(const RobotState *state)
{
    if (state != NULL) {
        last_frame = *state;
    }
}

RobotState display_stub_last_frame(void)
{
    return last_frame;
}
