#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include "task.h"
#include "idt.h"

void scheduler_tick(struct registers *r);
void scheduler_init(void);
void scheduler_add(task_t *task);
void _scheduler_remove_task();
void _set_scheduler_on();
void scheduler_set_task_state(task_state_t state);
void scheduler_wake_task(uint32_t pid);
int scheduler_get_idx_off_pid(uint32_t pid);
int scheduler_set_current_task(uint32_t pid);
void scheduler_yield(struct registers *r);
int  scheduler_get_task_count();
int scheduler_find_next_task();
int scheduler_find_first_task_based_on_state(task_state_t state);
task_t *scheduler_get_current_task();
int scheduler_has_runnable_task();

#endif