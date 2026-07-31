#ifndef OP_SY_H
#define OP_SY_H

#include <stdint.h>

int set_operator_task();
int ch_act_window(int target_pid);
int get_ac_tasks(char *list_buf, int len);
int exec(const char *filename);
int kill_task(uint32_t target_pid);
void terminate_program();

#endif
