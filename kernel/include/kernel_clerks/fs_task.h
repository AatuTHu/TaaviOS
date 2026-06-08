#ifndef FS_TASK_H
#define FS_TASK_H

#include "config.h"
#include "klog.h"
#include "kmalloc.h"
#include "kstring.h"
#include "sched.h"
#include "task.h"
#include <stdint.h>

#define MAX_REQ_ENTRIES    50
#define MAX_FD_ENTRIES     256
#define INVALID_IDX        -1
#define free_starting_slot 3

typedef enum {
    PENDING,
    IN_PROGRESS,
    COMPLETE,
    TERMINATED,
    FAILED,
} fs_task_queue_status_t;

typedef struct fd_entry_t {
    uint32_t owner_pid;
    uint32_t fd;
    uint32_t cluster;
    uint32_t size;
    uint32_t curr_offset;
    uint32_t flags;
    uint8_t addributes;
} fd_entry_t;

typedef struct request_queue_t {
    uint32_t caller_pid;
    operations_t request_type;
    char path[128];
    char buf[512];
    uint32_t buffer_size;
    uint32_t fd;
    uint32_t flags;
    fs_task_queue_status_t status;
} request_queue_t;

void fs_init(const task_t *fs_task);
void fs_task_loop();
void fs_recovery();
void fs_handle_request(request_queue_t *req);
int collect_request(uint32_t pid, char *out);
int add_request_to_queue(uint32_t pid, operations_t type, uint32_t fd,
    const char *path, const char *buf,
    uint32_t buffer_size, uint32_t flags);
void fs_wake_task(uint32_t pid);

extern int request_queue_count;
extern int current_req_index;
extern request_queue_t *request_queue[MAX_REQ_ENTRIES];
extern fd_entry_t *fd_entry_table[MAX_FD_ENTRIES];

#endif