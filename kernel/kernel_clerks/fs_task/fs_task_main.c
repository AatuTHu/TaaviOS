#include "blankie.h"
#include "fs_task.h"
#include "hail_mary.h"

/**
 * Fs_task
 * Design & Implementation:
 * @author: A.H, 2026
 */

fd_entry_t *fd_entry_table[MAX_FD_ENTRIES];

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
    // DEBUG_FS_TASK("[FS_TASK]: \n");
    while (1) {
        request_table *req = ledger_fetch_next_req(fs_task_pid);

        if (req != NULL) {
            if (req->status == PENDING ||
                req->status == IN_PROGRESS) {
                //__asm__ __volatile__("cli");
                fs_handle_request(req);
                //__asm__ __volatile__("sti");
            }
        }
        fs_maintain_virt_dir();

        /*
         * if(virt file needs servicing)
         * calculate next possible cluster/file
         * close / delete
         */

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
    DEBUG_FS_TASK("[FS_TASK][RECOVERY]: PROTOCOL HAIL MARY LAUNCHED\n");
    ledger_check_request(fs_task_pid);
    blankie_activate(fs_task_pid);
}

void fs_init(const task_t *fs_task) {
    register_hail_mary_function(fs_task_pid, fs_recovery);
    blankie_register(fs_task_pid, fs_task->context.eip, fs_task->kernel_stack);
}