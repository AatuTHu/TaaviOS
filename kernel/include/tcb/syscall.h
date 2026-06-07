#ifndef SYSCALL_H
#define SYSCALL_H
#include "idt.h"
#include <stdint.h>

#define MAX_SYSCALLS 256

#define SYS_EXIT   1
#define SYS_READ   3
#define SYS_WRITE  4
#define SYS_OPEN   5
#define SYS_CLOSE  6
#define SYS_GETPID 20
#define SYS_EXEC   11
#define SYS_IDLE   112
#define SYS_YIELD  158

typedef int32_t (*syscall_fn_t)(struct registers *r);
void syscall_init();
void syscall_dispatch(struct registers *r);

#endif