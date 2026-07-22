#ifndef SYSCALL_H
#define SYSCALL_H
#include "idt.h"
#include <stdint.h>

typedef int32_t (*syscall_fn_t)(struct registers *r);
void syscall_init();
void syscall_dispatch(struct registers *r);

#endif