#ifndef PROC_H
#define PROC_H

#include <stdint.h>
#include <stddef.h>
#include "paging.h"

#define MAX_PROCESSES 256

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
} context_t;

typedef struct {
    uint32_t                pid;
    process_state_t         state;
    page_directory_t        *page_dir;
    uint32_t                kernel_stack;
    uint32_t                user_stack;
    context_t               context;
    uint32_t                cs;
    uint32_t                ss;
} process_t;

process_t *process_create(uint32_t entry);
void       process_destroy(process_t *proc);
process_t *process_get(int index);


#endif