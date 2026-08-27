#include "op_sy.h"
#include "malloc.h"
#include "render.h"
#include "shared.h"
#include "string.h"
#include "sys_calls.h"
#include <stdint.h>

int get_ac_tasks(char *buf, int len) {
    if (buf != NULL) {
        memcpy(buf, "SYS_INFO/TASKS", len);
        return sys_getdirents(buf, len);
    }
    return -1;
}

int set_operator_task() {
    return sys_ioctl(SET_OPERATOR, 0, 0);
}

int ch_act_window(int target_pid) {
    return sys_ioctl(CH_ACT_W, target_pid, 0);
}

int exec(const char *filename) {
    return sys_exec(filename);
}

void terminate_program() {
    sys_exit();
}

int kill_task(uint32_t target_pid) {
    return sys_kill(target_pid);
}

int release_window() {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode = FREE;
    return sys_conwi(&params);
}

int __init_task() {

    int heap_start       = sys_sbrk(0);
    int current_heap_end = sys_sbrk(64);

    if (heap_start == -1 || current_heap_end == -1) {
        return -1;
    }

    malloc_init((void *)heap_start, current_heap_end);

    if (gfx_init() == -1) {
        return -1;
    }

    return 0;
}
