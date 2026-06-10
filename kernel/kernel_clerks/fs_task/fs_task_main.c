#include "blankie.h"
#include "fs_task.h"
#include "hail_mary.h"

/*
 * Fs_task
 * Design & Implementation: A.H, 2026
 */

fd_entry_t *fd_entry_table[MAX_FD_ENTRIES];

void fs_wake_task(uint32_t pid) {
    scheduler_wake_task(pid);
}

/**
 * fs_task_loop - entry point and the work algo.
 * Context: So there would be a work loop.
 */
/*
 * Main task loop of fs_task the algorithm
 */
void fs_task_loop() {
    // DEBUG("[FS_TASK]: \n");

    while (1) {
        request_table *request = fetch_next_task(fs_task_pid);

        if (request != NULL) {
            if (request->status == PENDING || request->status == IN_PROGRESS) {
                __asm__ __volatile__("cli");
                DEBUG("[FS_TASK][LOOP]: handling request\n");
                fs_handle_request(request);
                __asm__ __volatile__("sti");
            }
        }

        /*
         * if(virt file needs servicing)
         * calculate next possible cluster/file
         * close / delete
         */
    }
}

void fs_recovery() {
    for (int i = 0; i < MAX_REQ_ENTRIES; i++) {
        if (request_queue[fs_task_pid][i] != NULL &&
            (request_queue[fs_task_pid][i]->status == IN_PROGRESS)) {
            scheduler_wake_task(request_queue[fs_task_pid][i]->caller_pid);
            request_queue[fs_task_pid][i]->status = TERMINATED;
        }
    }
    blankie_activate(fs_task_pid);
}

void fs_init(const task_t *fs_task) {
    register_hail_mary_function(fs_task_pid, fs_recovery);
    blankie_register(fs_task_pid, fs_task->context.eip, fs_task->kernel_stack);
}