#ifndef FS_TASK_H
#define FS_TASK_H

#include <stdint.h>
#include "config.h"

typedef enum  {
    PENDING,
    IN_PROGRESS,
    COMPLETE,
} fs_task_queue_status_t;

typedef struct fs_task_queue_t {
    uint32_t caller_pid;
    uint8_t  request_type;
    char     path[128];
    uint32_t fd;
    uint8_t  status;
    
} fs_task_queue_t;

void fs_init();
void fs_task_loop();

#endif