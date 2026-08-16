#include "blankie.h"
#include "fs_clerk.h"
#include "hail_mary.h"
#include "klog.h"

/**
 * Fs_task
 * Design & Implementation:
 * @author: A.H, 2026
 */

#define FS_MAINTENANCE_INTERVAL 1000
fd_entry_t *fd_entry_table[MAX_FD_ENTRIES];
static uint32_t maintenance_counter = 0;

/**
 * fs_task_loop - Entry point function used when clerk is created at boot.
 *
 * Description:
 * When a request is made by userspace task fs_task is woken and start handling
 * requests from this function afeter all the requests have been handled it goes
 * to thru the blankie protocol
 *
 * Context: Runs besides other tasks to achieve asynchronous feeling.
 */
void fs_task_loop() {
    DEBUG_FS_TASK("[FS_TASK]: \n");
    fs_maintain_virt_dir();
    while (1) {
        request_table *req = ledger_fetch_next_req(fs_task_pid);

        if (req != NULL) {
            if (req->status == PENDING || req->status == IN_PROGRESS) {
                fs_handle_request(req);
            }
        }

        if (++maintenance_counter >= FS_MAINTENANCE_INTERVAL) {
            fs_maintain_virt_dir();
            maintenance_counter = 0;
        }

        blankie_activate(fs_task_pid);
    }
}

/**
 * fs_recovery - tries to remove malicious request.
 *
 * Description:
 * In a case where fs_task has made a critical error we come here via isr
 * handler to try remove malicious request then reset stack via blankie
 *
 * Context: to remove request and wake the caller.
 */
static void fs_recovery() {
    ERROR("[FS_TASK][RECOVERY]: PROTOCOL HAIL MARY LAUNCHED\n");
    ledger_check_request(fs_task_pid);
    blankie_activate(fs_task_pid);
}

void fs_init(const task_t *fs_task) {
    register_hail_mary_function(fs_task_pid, fs_recovery);
    blankie_register(fs_task_pid, fs_task->context.eip, fs_task->kernel_stack);
}
