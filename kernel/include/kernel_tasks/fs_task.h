#ifndef FS_TASK_H
#define FS_TASK_H

#include <stdint.h>

typedef struct fs_task_t {
    uint32_t mem_start;
    uint32_t mem_end;
} fs_task_t;

void fs_init();
void fs_task_loop();

#endif