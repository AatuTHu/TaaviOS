#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include "proc.h"
#include "idt.h"

void scheduler_tick(struct registers *r);
void scheduler_switch_context(struct registers *r, int idx);
void scheduler_init(void);
void scheduler_add(proc_t *proc);
void scheduler_kill_task();
void _set_scheduler_on();
void _scheduler_task_remove();
int scheduler_find_next();
int  scheduler_get_task_count();
proc_t *scheduler_get_current();

#endif