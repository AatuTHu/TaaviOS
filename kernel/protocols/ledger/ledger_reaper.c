#include "config.h"
#include "klog.h"
#include "kmalloc.h"
#include "kstring.h"
#include "ledger.h"
#include "sched.h"
#include <stdint.h>

int ledger_add_reaper_req(uint32_t caller_pid, uint32_t target_pid) {

    if (caller_pid >= MAX_TASKS || target_pid >= MAX_TASKS) {
        ERROR("[LEDGER][ADD_REAPER_REQUEST]: Callers pid or target pid was invalid. Aborting\n");
        return STATUS_ERROR;
    }

    request_table *new_request = (request_table *)kmalloc(sizeof(request_table));
    if (new_request == NULL) {
        ERROR("[LEDGER][ADD_REAPER_REQUEST]: could not allocate new request. Aborting\n");
        goto case_error;
    }

    memset(new_request, 0, sizeof(request_table));
    new_request->caller_pid = caller_pid;
    new_request->target_pid = target_pid;
    new_request->status     = PENDING;
    new_request->clerk_pid  = reaper_task_pid;

    DEBUG("[LEDGER][ADD_REAPER_REQUEST] Request added\n");
    if (ledger_enqueue(reaper_task_pid, new_request) == STATUS_ERROR) {
        ERROR("[LEDGER][ADD_REAPER_REQUEST]: Failed to add to req queue\n");
        kfree(new_request);
        return STATUS_ERROR;
    }

    return STATUS_OK;

case_error:
    scheduler_wake_task(caller_pid);
    return STATUS_ERROR;
}
