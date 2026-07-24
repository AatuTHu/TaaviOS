#include "config.h"
#include "klog.h"
#include "ledger.h"

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
int ledger_add_fs_req(uint32_t caller_pid, operations_t type, uint32_t fd, const char *path, const char *buf, uint32_t buffer_size, uint32_t flags) {

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
    new_request->fd           = fd;
    new_request->buffer_size  = buffer_size;

    if (type == WRITE && buf != NULL) {
        strncpy(new_request->buf, buf, sizeof(new_request->buf));
        // DEBUG_LEDGER("[LEDGER][ADD_FS_REQUEST]: buf: %s and buf length: %d\n",
        //     new_request->buf, buffer_size);
    }

    if (path != NULL && (type == OPEN || type == FIND || type == CREATE || type == LIST)) {
        strncpy(new_request->path, path, sizeof(new_request->path) - 1);
        new_request->path[sizeof(new_request->path) - 1] = '\0';
        // DEBUG_LEDGER("[LEDGER][ADD_FS_REQUEST]: path: %s and buf length: %d\n", new_request->path, buffer_size);
    }

    DEBUG_LEDGER("[LEDGER][ADD_FS_REQUEST] Request added\n");
    // DEBUG_LEDGER("[LEDGER][ADD_FS_REQUEST]: caller pid: %d\n", new_request->caller_pid);
    //  DEBUG_LEDGER("[LEDGER][ADD_FS_REQUEST]: clerk_pid: %d\n", new_request->clerk_pid);
    // DEBUG_LEDGER("[LEDGER][ADD_FS_REQUEST]: request_type: %d\n", new_request->request_type);
    // DEBUG_LEDGER("[LEDGER][ADD_FS_REQUEST]: fd: %d\n", new_request->fd);
    //  DEBUG_LEDGER("[LEDGER][ADD_FS_REQUEST]: buffer length : %d\n", new_request->buffer_size);
    //  DEBUG_LEDGER("[LEDGER][ADD_FS_REQUEST]: flags: %d\n", new_request->flags);

    if (ledger_enqueue(fs_task_pid, new_request) == STATUS_ERROR) {
        ERROR("[LEDGER][ADD_FS_REQUEST]: Failed to add to req queue\n");
        kfree(new_request);
        return STATUS_ERROR;
    }

    return STATUS_OK;

case_error:
    scheduler_wake_task(caller_pid);
    return STATUS_ERROR;
}
