#ifndef PROC_H
#define PROC_H

#include <stdint.h>
#include <stddef.h>
#include "paging.h"
#include "idt.h"

typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_DEAD
} process_state_t;


typedef struct proc_t {
    uint32_t                pid;
    char                    name[32];
    process_state_t         state;
    page_directory_t        *page_dir;
    uint32_t                kernel_stack;
    uint32_t                useresp;
    uint8_t                 started;
    struct registers        context;
    struct proc_t           *next;
} proc_t;

proc_t *process_create(uint32_t entry, const char *name);
void       process_destroy(proc_t *proc);
proc_t *process_get(int index);


#endif