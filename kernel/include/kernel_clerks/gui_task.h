#ifndef GUI_TASK_H
#define GUI_TASK_H

#include "stdint.h"
#include "task.h"

typedef struct blueprint {
    uint32_t *pixels;
    uint32_t owner_pid;
    uint32_t width;
    uint32_t height;
    uint32_t screen_x;
    uint32_t screen_y;
} blueprint_t;

void gui_task_loop();
void gui_init(task_t *gui_task);

#endif
