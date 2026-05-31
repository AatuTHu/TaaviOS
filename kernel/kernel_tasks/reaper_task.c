#include "reaper_task.h"
#include "sched.h"
#include "blankie.h"
#include "klog.h"

/*
 * Reaper_task
 * Design & Implementation: A.H, 2026
*/

/*
* This file contains the implementation of the reaper. Its job is to delete dead things. For now it's only doing it to schedulers dead tasks
* but it can be expanded on.
*
* Otherwise it follows the blankie_protocol
*/

void reaper_task_loop() {
    while(1) {
        _scheduler_remove_task();

        blankie_activate(reaper_task_pid);
    }
}

void reaper_init(task_t *reaper_task) {
    blankie_register(reaper_task_pid, reaper_task->context.eip, reaper_task->kernel_stack);
}