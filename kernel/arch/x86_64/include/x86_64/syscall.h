#ifndef SYSCALL_H
#define SYSCALL_H
#include "idt.h"
#include <stdint.h>

typedef int64_t (*syscall_fn_t)(struct registers *r);
int64_t sys_window(struct registers *r);
void syscall_init();
void syscall_dispatch(struct registers *r);

#endif
