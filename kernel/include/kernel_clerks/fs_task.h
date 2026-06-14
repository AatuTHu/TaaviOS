#ifndef FS_TASK_H
#define FS_TASK_H

#include "config.h"
#include "klog.h"
#include "kmalloc.h"
#include "kstring.h"
#include "ledger.h"
#include "sched.h"
#include "task.h"
#include <stdint.h>

#define INVALID_IDX        -1
#define free_starting_slot 3
#define backwards          0
#define forwards           1
/*
typedef enum {
    PENDING,
    IN_PROGRESS,
    COMPLETE,
    TERMINATED,
    FAILED,
} reqistry_status;

typedef struct request_table {
    uint32_t caller_pid;
    operations_t request_type;
    char path[128];
    char buf[512];
    uint32_t buffer_size;
    uint32_t fd;
    uint32_t flags;
    reqistry_status status;
} request_table;*/

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

typedef struct dir_traversal_t {
    uint32_t owner_pid;
    uint32_t current_cluster;
    uint32_t prev_cluster;
} dir_traversal_t;

void fs_init(const task_t *fs_task);
void fs_task_loop();
void fs_recovery();
void fs_wake_task(uint32_t pid);
void fs_handle_request(request_table *req);
/*
int fs_add_reqs(uint32_t caller_pid,
    operations_t type, uint32_t fd, const char *path,
    const char *buf, uint32_t buffer_size, uint32_t flags);
int fs_collect_req(uint32_t caller_pid, char *out);*/

extern fd_entry_t *fd_entry_table[MAX_FD_ENTRIES];
// extern request_table *request_queue[MAX_REQ_ENTRIES];
// extern int request_queue_count;

#endif