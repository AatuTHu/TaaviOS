#ifndef FS_TASK_H
#define FS_TASK_H

#include <stdint.h>
#include "config.h"
#include "task.h"
#include "sched.h"
#include "klog.h"
#include "kstring.h"
#include "kmalloc.h"

#define INVALID_IDX -1
#define free_starting_slot 3
typedef enum  {
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
} fd_entry_t;

typedef struct fs_mailbox_queue {
    uint32_t                caller_pid;
    operations_t            request_type;
    char                    path[128];
    char                    buf[512];
    uint32_t                buffer_size;
    uint32_t                fd;
    uint32_t                flags;
    fs_task_queue_status_t  status;
} fs_mailbox_queue;

void fs_init(const task_t *fs_task);
void fs_task_loop();
void fs_recovery();
void fs_handle_request(fs_mailbox_queue *req);
int collect_request(uint32_t pid, char* out);
int add_request_to_queue(uint32_t pid, operations_t type, uint32_t fd, const char* path, const char *buf, uint32_t buffer_size, uint32_t flags);


extern int request_queue_count;
extern int current_req_index;
extern fs_mailbox_queue *request_queue[MAX_TASKS];
extern fd_entry_t *fd_entry_table[MAX_TASKS];

#endif