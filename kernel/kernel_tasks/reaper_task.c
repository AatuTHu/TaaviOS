#include "reaper_task.h"
#include "sched.h"
#include "blankie.h"
#include "klog.h"


void reaper_task_loop() {
    while(1) {
        _scheduler_remove_task();

        blankie_activate(reaper_task_pid);
    }
}

void reaper_init(task_t *reaper_task) {
    blankie_register(reaper_task_pid, reaper_task->context.eip, reaper_task->kernel_stack);
}