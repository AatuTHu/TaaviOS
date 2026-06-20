#ifndef GUI_TASK_H
#define GUI_TASK_H

#include "stdint.h"
#include "task.h"

typedef struct window {
    uint32_t wid;
    uint32_t owner_pid;
    uint32_t width;
    uint32_t height;
    uint32_t z_index;
    uint32_t x_offset;
    uint32_t y_offset;
    uint32_t fg_color;
    uint32_t bg_color;
} window_t;

// int gui_draw_string(const char *string, uint32_t caller_pid);
int gui_change_fg_color(uint32_t fg_color);
int gui_change_bf_color(uint32_t bg_color);
int gui_set_active_window(uint32_t wid);
int gui_create_window(uint32_t owner_pid, uint32_t width, uint32_t height);

void gui_task_loop();
void qui_recovery();
void gui_init(task_t *gui_task);

#endif