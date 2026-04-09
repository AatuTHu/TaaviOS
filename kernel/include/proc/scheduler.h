#ifndef SCHEDULER_H
#define SCHEDULER_H
#include <stdint.h>
#include "proc.h"
#include "idt.h"

proc_t *scheduler_get_current(void);
void scheduler_init(void);
void scheduler_add(proc_t *proc);
void scheduler_tick(struct registers *r);
void scheduler_remove(void);

#endif