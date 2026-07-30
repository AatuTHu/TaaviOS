#ifndef GUI_TASK_H
#define GUI_TASK_H

#include "stdint.h"
#include "task.h"

typedef struct window_t {
    uint32_t wid;
    uint32_t owner_pid;
    uint32_t width;
    uint32_t height;
    uint32_t z_index;
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t *pixels;
} window_t;

typedef struct blueprint_t {
    uint32_t screen_x;
    uint32_t screen_y;
    window_t *entry;
} blueprint_t;

void gui_task_loop();
void gui_init(task_t *gui_task);

#endif
