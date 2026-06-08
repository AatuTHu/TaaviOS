#ifndef SCHED_H
#define SCHED_H

#include "idt.h"
#include "task.h"
#include <stdint.h>

void scheduler_tick(struct registers *r);
void scheduler_init(void);
void scheduler_add(task_t *task);
void scheduler_remove_task();
int scheduler_get_dead_task_count();
void _set_scheduler_on();
void scheduler_set_task_state(task_state_t state);
void scheduler_wake_task(uint32_t pid);
int scheduler_set_current_task(uint32_t pid);
void scheduler_yield(struct registers *r);
task_t *scheduler_get_current_task();
int scheduler_has_runnable_task();

#endif