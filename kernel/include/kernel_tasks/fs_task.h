#ifndef FS_TASK_H
#define FS_TASK_H

#include <stdint.h>
#include "config.h"

typedef enum  {
    PENDING,
    IN_PROGRESS,
    COMPLETE,
} fs_task_queue_status_t;

typedef struct fd_entry_t {
    uint32_t owner_pid;
    uint32_t fd;
    uint32_t cluster;
    uint32_t size;
    uint32_t curr_offset;
} fd_entry_t;

typedef struct fs_mailbox_queue {
    uint32_t caller_pid;
    uint8_t  request_type;
    char     path[128];
    char    *buf;
    uint32_t fd;
    uint8_t  status;
    
} fs_mailbox_queue;

void fs_init();
void fs_task_loop();
int add_request_to_queue(uint32_t pid, uint8_t request_type,uint32_t fd, const char* path, char *buf);

#endif