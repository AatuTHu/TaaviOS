#include "reaper_task.h"
#include "blankie.h"
#include "config.h"
#include "hail_mary.h"
#include "klog.h"
#include "ledger.h"
#include "sched.h"

/*
 * Reaper_task
 * Design & Implementation: A.H, 2026
 */

/*
 * This file contains the implementation of the reaper. Its job is to delete
 * dead things. For now it's only doing it to schedulers dead tasks but it can
 * be expanded on.
 *
 * As does other clerks it follows the blankie_protocol
 */

void reaper_task_loop() {
    while (1) {
        DEBUG("[REAPER][LOOP]: Killing tasks\n");
        scheduler_remove_task();
        DEBUG("[REAPER][LOOP]: Killing requests\n");
        ledger_remove_request();
        DEBUG("[REAPER][LOOP]: Going to sleep\n");
        blankie_activate(reaper_task_pid);
    }
}

static void reaper_recovery() {
    DEBUG("[REAPER][RECOVERY]:\n");
    blankie_activate(reaper_task_pid);
}

void reaper_init(const task_t *reaper_task) {
    blankie_register(reaper_task_pid, reaper_task->context.eip,
                     reaper_task->kernel_stack);
    register_hail_mary_function(reaper_task_pid, reaper_recovery);
}