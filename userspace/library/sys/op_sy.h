#ifndef OP_SY_H
#define OP_SY_H

#include "shared.h"
#include <stdint.h>

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
int draw_buffer(int width, int height, int x, int y, uint32_t scale, uint32_t *sprite);
int kill_task(uint32_t target_pid);
#endif
