#ifndef GUI_TASK_H
#define GUI_TASK_H

#include "task.h"
#include <stdint.h>

typedef struct blueprint {
    uint32_t *pixels;
    uint16_t owner_pid;
    uint16_t width;
    uint16_t height;
    uint16_t screen_x;
    uint16_t screen_y;
} blueprint_t;

void gui_task_loop();
void gui_init(const task_t *gui_task);

#endif
