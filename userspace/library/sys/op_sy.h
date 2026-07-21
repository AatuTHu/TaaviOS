#ifndef OP_SY_H
#define OP_SY_H

#include "defines.h"

typedef enum {
    W_CREATE,
    W_PAINT,
    W_MOVE,
    W_SET_OPERATOR,
    W_CH_ACT_W,
    W_CH_BG_COLOR,
    W_CH_FG_COLOR,
} window_operations;

int set_operator_task();
int ch_act_window(int target_pid);
int get_ac_tasks(char *list_buf, int len);
int ch_bg_color(int color);
int ch_fg_color(int color);
int paint_window(int width, int height, int x, int y);
int exec(const char *filename);
void terminate_program();
int create_task_window(int width, int height, int x, int y);
int move_task_window(int x, int y);

#endif