#include "blankie.h"
#include "fs_task.h"
#include "hail_mary.h"

/**
 * Fs_task
 * Design & Implementation:
 * @author: A.H, 2026
 */

int request_queue_count = 0;
int current_req_index   = -1;
request_table *request_queue[MAX_REQ_ENTRIES];
fd_entry_t *fd_entry_table[MAX_FD_ENTRIES];

// signal scheduler to wake the pid who made the request.
void fs_wake_task(uint32_t pid) {
    scheduler_wake_task(pid);
}

/**
 * fs_remove_from_queue - used main loop when picks a terminated requst.
 * @req: pointer to a terminated request
 *
 * Description:
 * frees the request from heap memory, then shifts there request_queue
 * array to left by one.
 *
 */
static void fs_remove_from_queue(request_table *req) {
    // DEBUG("[FS_TASK][REMOVE]: Starting on removing\n");
    if (request_queue_count == 0 || current_req_index == -1) {
        // DEBUG("[FS_TASK][REMOVE]: Req queue count is 0\n");
        return;
    }

    request_queue[current_req_index] = NULL;
    kfree(req);

    for (int i = current_req_index; i < request_queue_count - 1; i++) {
        request_queue[i] = request_queue[i + 1];
    }
    request_queue[request_queue_count - 1] = NULL;
    request_queue_count--;
    current_req_index = -1;

    // DEBUG("[FS_TASK][REMOVE]: Freeing request heap memory\n");
    // DEBUG("[FS_TASK][REMOVE]: Removing complete\n");
}

/*
 * This functions was inspired from schedulers next task find function.
 * It tries to find the index of the first in_progress request,
 * if none is found it tries to find pending. Lastly it tries to find a
 * terminated req it can be cleaned away. If it find any of these it returns
 * index of the selected request
 */

/**
 * find_next_request - Simple round-robin picker.
 *
 * Description:
 * loops thru request_queue. Tries to find requests in order of
 * "IN_PROGRESS" -> "PENDIG" -> "TERMINATED"
 *
 * Context: picks a request for fs_task at the start of loop iteration.
 * Return: Index of next request or INVALID_IDX on failure.
 */
static int find_next_request() {
    if (request_queue_count == 0)
        return STATUS_ERROR;

    for (int i = 0; i < request_queue_count; i++) {
        if (request_queue[i] != NULL &&
            request_queue[i]->status == IN_PROGRESS) {
            return current_req_index = i;
        }
    }

    for (int i = 0; i < request_queue_count; i++) {
        if (request_queue[i] != NULL && request_queue[i]->status == PENDING) {
            return current_req_index = i;
        }
    }

    for (int i = 0; i < request_queue_count; i++) {
        if (request_queue[i] != NULL &&
            request_queue[i]->status == TERMINATED) {
            return current_req_index = i;
        }
    }

    ////DEBUG("[FS_TASK][NEXT_REQUEST]: could not find new request\n");
    return INVALID_IDX;
}

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
    // DEBUG("[FS_TASK]: \n");
    while (1) {
        int index = find_next_request();
        if (request_queue_count > 0 && index != INVALID_IDX) {
            request_table *request = request_queue[index];
            if (request != NULL) {
                if (request->status == PENDING ||
                    request->status == IN_PROGRESS) {
                    //__asm__ __volatile__("cli");
                    // DEBUG("[FS_TASK][LOOP]: handling request\n");
                    fs_handle_request(request);
                    //__asm__ __volatile__("sti");
                }

                //__asm__ __volatile__("cli");
                if (request->status == TERMINATED) {
                    // DEBUG("[FS_TASK][LOOP]: Request is complete. Removing
                    // it\n");
                    fs_remove_from_queue(request);
                    request = NULL;
                }
                //__asm__ __volatile__("sti");
            }
        }

        /*
         * if(virt file needs servicing)
         * calculate next possible cluster/file
         * close / delete
         */

        if (request_queue_count == 0) {
            blankie_activate(fs_task_pid);
        }
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
void fs_recovery() {
    // DEBUG("[FS_TASK][RECOVERY]: PROTOCOL HAIL MARY LAUNCHED\n");
    if (current_req_index != -1) {
        request_table *req = request_queue[current_req_index];
        // DEBUG("[FS_TASK][RECOVERY]: No freeing needed\n");
        if (req != NULL) {
            scheduler_wake_task(req->caller_pid);
            fs_remove_from_queue(req);
        }
    }
    blankie_activate(fs_task_pid);
}

void fs_init(const task_t *fs_task) {
    register_hail_mary_function(fs_task_pid, fs_recovery);
    blankie_register(fs_task_pid, fs_task->context.eip, fs_task->kernel_stack);
}