#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include "proc.h"
#include "idt.h"

void scheduler_tick(struct registers *r);
void scheduler_block_task();
void scheduler_init(void);
void scheduler_add(proc_t *proc);
void scheduler_kill_task();
void _scheduler_remove_task();
void _set_scheduler_on();
void scheduler_set_task_ready();
void scheduler_set_task_sleeping();
void scheduler_wake_task(int pid);
void scheduler_switch_context(struct registers *r, int idx);
int  scheduler_get_task_count();
int scheduler_find_next_task();
int scheduler_find_first_task_based_on_state(process_state_t state);
int scheduler_does_exist(int pid);
int scheduler_get_idx_off_pid(int pid);
proc_t *scheduler_get_current_task();
int scheduler_has_runnable_task();

#endif