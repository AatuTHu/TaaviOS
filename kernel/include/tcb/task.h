#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stddef.h>
#include "paging.h"
#include "idt.h"

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_DEAD
} task_state_t;

typedef enum {
    PRIORITY_HIGH,
    PRIORITY_NORMAL,
    PRIORITY_LOW
} task_priority_t;

typedef struct task_t {
    uint32_t                pid;
    char                    name[32];
    task_state_t            state;
    page_directory_t        *page_dir;
    uint32_t                kernel_stack;
    uint8_t                 started;
    uint8_t                 priority;
    struct registers        context;
    uint8_t                 task_mode;
} task_t;

task_t *task_create(uint32_t entry, const char *name, page_directory_t *page_dir, uint8_t task_mode);
task_t *task_get(int index);
task_t *get_task_by_name(char *name);
void   task_destroy(task_t *task, uint8_t task_mode);


#endif