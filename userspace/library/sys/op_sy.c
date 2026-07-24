#include "op_sy.h"
#include "stand.h"
#include "string.h"
#include "sys_calls.h"

int get_ac_tasks(char *buf, int len) {
    return sys_getdirents(buf, "SYS_INFO/TASKS", len);
}

int create_task_window(int width, int height, int x, int y) {
    return sys_conwi(W_CREATE, width, height, x, y);
}

int paint_window(int width, int height, int x, int y) {
    return sys_conwi(W_PAINT, width, height, x, y);
}

int move_task_window(int x, int y) {
    return sys_conwi(W_MOVE, 0, 0, x, y);
}
int set_operator_task() {
    return sys_conwi(W_SET_OPERATOR, 0, 0, 0, 0);
}

int ch_act_window(int target_pid) {
    return sys_conwi(W_CH_ACT_W, target_pid, 0, 0, 0);
}

int ch_bg_color(int color) {
    return sys_conwi(W_CH_BG_COLOR, color, 0, 0, 0);
}
int ch_fg_color(int color) {
    return sys_conwi(W_CH_FG_COLOR, color, 0, 0, 0);
}

int draw_buffer(int width, int height, int x, int y, uint32_t scale, uint32_t *sprite) {
    gui_params_pack params;

    params.width  = width;
    params.height = height;
    params.x      = x;
    params.y      = y;
    params.scale  = scale;
    params.pixels = sprite;

    return sys_drwi(W_DRAW_BUFFER, &params);
}

int exec(const char *filename) {
    return sys_exec(filename);
}

void terminate_program() {
    sys_exit();
}
