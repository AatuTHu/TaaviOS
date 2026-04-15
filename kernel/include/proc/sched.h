#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include "proc.h"
#include "idt.h"

void scheduler_tick(struct registers *r);
void scheduler_init(void);
void scheduler_remove();
void scheduler_add(proc_t *proc);
void _set_scheduler_on();
int  scheduler_get_task_count();
proc_t *scheduler_get_current();

#endif