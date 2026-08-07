#ifndef TASK_H
#define TASK_H

#include "config.h"
#include "idt.h"
#include "paging.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_DEAD
} task_state_t;

typedef enum { PRIORITY_HIGH,
               PRIORITY_NORMAL,
               PRIORITY_LOW } task_priority_t;

typedef struct task_t {
    struct registers context;
    page_directory_t *page_dir;
    task_state_t state;
    char name[TASK_NAME_LENGTH];
    uint32_t pid;
    uint32_t kernel_stack;
    uint32_t heap_start;
    uint32_t heap_end;
    uint8_t started;
    uint8_t priority;
    uint8_t task_mode;
} task_t;

extern task_t *task_table[MAX_TASKS];
task_t *task_create(int reserved_pid, uint32_t entry, uint32_t heap_start, const char *name,
                    page_directory_t *page_dir, uint8_t task_mode);
task_t *task_get(uint32_t pid);
int task_destroy(task_t *task);
void task_sync_kernel_entries_to_all_tasks();
#endif
