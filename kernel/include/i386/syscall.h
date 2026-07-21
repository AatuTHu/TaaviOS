#ifndef SYSCALL_H
#define SYSCALL_H
#include "idt.h"
#include <stdint.h>

typedef enum {
    W_CREATE,
    W_PAINT,
    W_MOVE,
    W_SET_OPERATOR,
    W_CH_ACT_W,
    W_CH_BG_COLOR,
    W_CH_FG_COLOR,
} window_operations;

#define MAX_SYSCALLS 256

#define SYS_EXIT 1
#define SYS_READ 3
#define SYS_WRITE 4
#define SYS_OPEN 5
#define SYS_CLOSE 6
#define SYS_CHDIR 12
#define SYS_GETPID 20
#define SYS_EXEC 11
#define SYS_MKDIR 39
#define SYS_IDLE 112
#define SYS_GETDENTS 141
#define SYS_YIELD 158
#define SYS_CONWI 250

typedef int32_t (*syscall_fn_t)(struct registers *r);
void syscall_init();
void syscall_dispatch(struct registers *r);

#endif