#include "reaper_clerk.h"
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

void reaper_task_loop() {
    while (1) {
        request_table *req = ledger_fetch_next_req(reaper_clerk_pid);
        if (req != NULL && (req->status == PENDING || req->status == IN_PROGRESS)) {
            if (scheduler_remove_task(req->target_pid) != STATUS_TAL) {
                req->status = TERMINATED;
            }
        }

        if (ledger_has_killable_reqs() > 0) {
            ledger_remove_request();
        }

        blankie_activate(reaper_clerk_pid);
    }
}

static void reaper_recovery() {
    ERROR("[REAPER][RECOVERY]:\n");
    ledger_check_request(reaper_clerk_pid);
    blankie_activate(reaper_clerk_pid);
}

void reaper_init(const task_t *reaper_task) {
#if defined(__x86_64__)
    blankie_register(reaper_clerk_pid, reaper_task->context.rip, reaper_task->kernel_stack);
#else
    blankie_register(reaper_clerk_pid, reaper_task->context.eip,
                     reaper_task->kernel_stack);
#endif
    register_hail_mary_function(reaper_clerk_pid, reaper_recovery);
}
