#include "op_sy.h"
#include "render.h"
#include "shared.h"
#include "sys_calls.h"
#include <stdint.h>
#include <string.h>

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
