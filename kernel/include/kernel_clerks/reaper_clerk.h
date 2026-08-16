#ifndef REAPER_TASK_H
#define REAPER_TASK_H

#include "task.h"
#include <stdint.h>

void reaper_init(const task_t *reaper_task);
void reaper_task_loop();

#endif