#include "reaper_task.h"
#include "sched.h"
#include "blankie.h"
#include "klog.h"
#include "config.h"

/*
 * Reaper_task
 * Design & Implementation: A.H, 2026
*/

/*
* This file contains the implementation of the reaper. Its job is to delete dead things. For now it's only doing it to schedulers dead tasks
* but it can be expanded on.
*
* As does other clerks it follows the blankie_protocol
*/

void reaper_task_loop() {
    while(1) {
        int kills = scheduler_get_dead_task_count();
        
        if(kills == 0) {
            blankie_activate(reaper_task_pid);
        }
        
        
        DEBUG("[REAPER][LOOP]: kill_count %d\n", kills);
        if(scheduler_remove_task() == STATUS_ERROR) {
            DEBUG("[REAPER][LOOP]: Something went wrong with killing..\n");
        }
        
        kills--;
    }

}

void reaper_init(task_t *reaper_task) {
    blankie_register(reaper_task_pid, reaper_task->context.eip, reaper_task->kernel_stack);
}