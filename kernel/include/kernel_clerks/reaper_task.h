#ifndef REAPER_TASK_H
#define REAPER_TASK_H

#include <stdint.h>
#include "task.h"

void reaper_init(task_t *reaper_task);
void reaper_task_loop();

#endif