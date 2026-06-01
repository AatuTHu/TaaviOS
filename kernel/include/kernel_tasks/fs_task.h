#ifndef FS_TASK_H
#define FS_TASK_H

#include <stdint.h>
#include "config.h"
#include "task.h"

typedef enum  {
    PENDING,
    IN_PROGRESS,
    COMPLETE,
    TERMINATED,
} fs_task_queue_status_t;

typedef struct fd_entry_t {
    uint32_t owner_pid;
    uint32_t fd;
    uint32_t cluster;
    uint32_t size;
    uint32_t curr_offset;
} fd_entry_t;

typedef struct fs_mailbox_queue {
    uint32_t                caller_pid;
    operations_t            request_type;
    char                    path[128];
    char                    buf[512];
    uint32_t                fd;
    fs_task_queue_status_t  status;
} fs_mailbox_queue;

void fs_init(task_t *fs_task);
void fs_task_loop();
int collect_request(uint32_t pid, char* out);
int add_request_to_queue(uint32_t pid,operations_t type,uint32_t fd, const char* path,const char *buf);

#endif