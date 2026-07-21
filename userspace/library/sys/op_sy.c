#include "op_sy.h"
#include "stand.h"
#include "string.h"
#include "sys_calls.h"

int set_operator_task() {
    return sys_conwi(3, 0, 0, 0, 0);
}

int ch_act_window(int target_pid) {
    return sys_conwi(4, target_pid, 0, 0, 0);
}

int get_ac_tasks(char *buf, int len) {
    return sys_getdirents(buf, "SYS_INFO/TASKS", len);
}
