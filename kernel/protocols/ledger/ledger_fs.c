#include "config.h"
#include "klog.h"
#include "kmalloc.h"
#include "kstring.h"
#include "ledger.h"
#include "sched.h"
#include "shared.h"
#include <stddef.h>
#include <stdint.h>

/**
 * ledger_add_fs_req - makes a new entry req.
 * @caller_pid:  pid of the task making the request
 * @type:        operation type (READ, WRITE, OPEN, ...)
 * @fd:          file descriptor (ignored for OPEN/CREATE/FIND)
 * @path:        path string, used for OPEN/CREATE/FIND
 * @buf:         data buffer, used for WRITE
 * @buffer_size: size of buf / requested size
 * @flags:       open/access flags
 *
 * Description:
 * Validates parameters, allocates a new request_table entry, places it in
 * the correct clerk's queue and wakes that clerk.
 *
 * Return: STATUS_OK on success, STATUS_ERROR on failure.
 */
int ledger_add_fs_req(uint32_t caller_pid, operations_t type, uint32_t fd, const char *buf, uint32_t buffer_size, uint32_t flags) {

    if ((fd < 2 || fd > MAX_FD_ENTRIES) && (type == READ || type == WRITE)) {
        ERROR("[LEDGER][ADD_FS_REQUEST]: Invalid fd number. Aborting\n");
        goto case_error;
    }

    request_table *new_request = (request_table *)kmalloc(sizeof(request_table));
    if (new_request == NULL) {
        ERROR("[LEDGER][ADD_FS_REQUEST]: could not allocate new request. Aborting\n");
        goto case_error;
    }

    memset(new_request, 0, sizeof(request_table));
    new_request->caller_pid   = caller_pid;
    new_request->clerk_pid    = fs_task_pid;
    new_request->request_type = type;
    new_request->flags        = flags;
    new_request->status       = PENDING;
    new_request->struct_key   = fd;
    new_request->buffer_size  = buffer_size;

    if (buffer_size > 0) {
        new_request->buf = (char *)kmalloc(buffer_size + 1);
        if (new_request->buf != NULL) {

            if (buf != NULL) {
                memcpy(new_request->buf, buf, buffer_size);
            }
            new_request->buffer_size      = buffer_size;
            new_request->buf[buffer_size] = '\0';
            DEBUG_FS_TASK("[LEDGER][ADD_FS_REQUEST] buffer: %s\n", new_request->buf);
        }
    }
    DEBUG_FS_TASK("[LEDGER][ADD_FS_REQUEST]: caller pid: %d\n", new_request->caller_pid);
    DEBUG_FS_TASK("[LEDGER][ADD_FS_REQUEST]: clerk_pid: %d\n", new_request->clerk_pid);
    DEBUG_FS_TASK("[LEDGER][ADD_FS_REQUEST]: request_type: %d\n", new_request->request_type);
    DEBUG_FS_TASK("[LEDGER][ADD_FS_REQUEST]: fd: %d\n", new_request->struct_key);
    DEBUG_FS_TASK("[LEDGER][ADD_FS_REQUEST]: buffer length : %d\n", new_request->buffer_size);
    DEBUG_FS_TASK("[LEDGER][ADD_FS_REQUEST]: flags: %d\n", new_request->flags);

    if (ledger_enqueue(fs_task_pid, new_request) == STATUS_ERROR) {
        ERROR("[LEDGER][ADD_FS_REQUEST]: Failed to add to req queue\n");
        kfree(new_request);
        scheduler_wake_task(caller_pid);
        return STATUS_ERROR;
    }

    DEBUG_FS_TASK("[LEDGER][ADD_FS_REQUEST] Request added\n");
    return STATUS_OK;

case_error:
    scheduler_wake_task(caller_pid);
    return STATUS_ERROR;
}

int ledger_add_fs_free_req(uint32_t caller_pid, uint32_t target_pid) {
    if (caller_pid > MAX_TASKS) {
        return STATUS_ERROR;
    }

    request_table *new_request = (request_table *)kmalloc(sizeof(request_table));

    if (new_request == NULL) {
        ERROR("[LEDGER][FS_ADD_FREE_REQ]: Could not allocate memory for the request\n");
        return STATUS_ERROR;
    }

    memset(new_request, 0, sizeof(request_table));
    new_request->caller_pid   = caller_pid;
    new_request->target_pid   = target_pid;
    new_request->request_type = FREE;
    new_request->clerk_pid    = fs_task_pid;

    if (ledger_enqueue(fs_task_pid, new_request) == STATUS_ERROR) {
        ERROR("[LEDGER[ADD_FS_FREE_REQUEST]: Failed to add req to queue\n");
        kfree(new_request);
        scheduler_wake_task(caller_pid);
        return STATUS_ERROR;
    }

    return STATUS_OK;
}
