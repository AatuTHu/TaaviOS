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

static int reaper_kill_task(uint32_t target_pid) {

    if (target_pid > MAX_TASKS) {
        return STATUS_ERROR;
    }

    if (scheduler_remove_task(target_pid) == STATUS_ERROR) {
        ERROR("[REAPER][HANDLE_REQUEST]: Failed to remove task\n");
        return STATUS_ERROR;
    }

    return STATUS_OK;
}

/*
 * This file contains the implementation of the reaper. Its job is to delete
 * dead things. For now it's only doing it to schedulers dead tasks but it can
 * be expanded on.
 *
 * As does other clerks it follows the blankie_protocol
 */
void reaper_task_loop() {
    while (1) {
        request_table *req = ledger_fetch_next_req(reaper_task_pid);
        if (req != NULL) {
            if (req->status == PENDING || req->status == IN_PROGRESS) {
                req->status = (reaper_kill_task(req->target_pid) == STATUS_OK) ? COMPLETE : FAILED;
                scheduler_wake_task(req->caller_pid);
            }
        } else {
            ledger_remove_request();
        }
        blankie_activate(reaper_task_pid);
    }
}

static void reaper_recovery() {
    DEBUG("[REAPER][RECOVERY]:\n");
    ledger_check_request(reaper_task_pid);
    blankie_activate(reaper_task_pid);
}

void reaper_init(const task_t *reaper_task) {
    blankie_register(reaper_task_pid, reaper_task->context.eip,
                     reaper_task->kernel_stack);
    register_hail_mary_function(reaper_task_pid, reaper_recovery);
}
