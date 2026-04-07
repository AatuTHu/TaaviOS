#ifndef PROC_H
#define PROC_H

#include <stdint.h>
#include <stddef.h>
#include "paging.h"

typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_DEAD
} process_state_t;

typedef struct {
    uint32_t edi, esi, ebx, edx, ecx, eax;
    uint32_t ebp, esp, eip;
    uint32_t eflags;
    uint32_t cs, ss;
} context_t;

typedef struct proc_t {
    uint32_t                pid;
    char                    name[32];
    process_state_t         state;
    page_directory_t        *page_dir;
    uint32_t                kernel_stack;
    uint32_t                user_stack;
    context_t               context;
    struct proc_t           *next;
} proc_t;

proc_t *process_create(uint32_t entry, const char *name);
void       process_destroy(proc_t *proc);
proc_t *process_get(int index);


#endif