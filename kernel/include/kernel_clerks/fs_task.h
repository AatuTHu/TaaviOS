#ifndef FS_TASK_H
#define FS_TASK_H

#include "config.h"
#include "klog.h"
#include "kmalloc.h"
#include "kstring.h"
#include "sched.h"
#include "task.h"
#include <stdint.h>

#define INVALID_IDX        -1
#define free_starting_slot 3

typedef struct fd_entry_t {
    uint32_t owner_pid;
    uint32_t fd;
    uint32_t file_cluster;
    uint32_t dir_cluster;
    uint32_t size;
    uint32_t curr_offset;
    uint32_t flags;
    uint8_t attr;
} fd_entry_t;

void fs_init(const task_t *fs_task);
void fs_task_loop();
void fs_recovery();
void fs_wake_task(uint32_t pid);

extern int request_queue_count;
extern int current_req_index;
extern fd_entry_t *fd_entry_table[MAX_FD_ENTRIES];

#endif