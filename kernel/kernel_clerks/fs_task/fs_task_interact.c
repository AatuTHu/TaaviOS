#include "fs_task.h"

/**
 * Fs_task
 * Design & Implementation:
 * @author: A.H, 2026
 */

/**
 * add_request_to_queue - A noticeboard to pin request.
 * @pid: caller's pid to attach to request
 * @type: type of requst
 * @fd: file descriptor number
 * @path: path to a file
 * @buf: buffer containing data
 * @buffer_size: -
 * @flags: flags to set when opening a file
 *
 * Description:
 * Makes a new request of the given params. Then add that request to queue.
 * After that wakes fs_task and sets its priority as high.
 *
 * Context: To achieve asynchronity between tasks No direct contact with fs_task
 * is needed. Return: STATUS_OK on succesful entry added and STATUS_ERROR if
 * failed.
 */
int fs_add_reqs(uint32_t caller_pid,
    operations_t type, uint32_t fd, const char *path,
    const char *buf, uint32_t buffer_size, uint32_t flags) {

    request_table *new_request = (request_table *)kmalloc(sizeof(request_table));
    if (new_request == NULL) {
        ERROR("[FS_TASK][ADD_REQUEST]: could not allocate new request. Aborting\n");
        goto case_error;
    }

    if ((fd < 2 || fd > MAX_FD_ENTRIES) && (type != OPEN && type != CREATE && type != FIND)) {
        ERROR("[FS_TASK][ADD_REQUEST]: Invalid fd number. Aborting\n");
        goto case_error;
    }

    new_request->caller_pid   = caller_pid;
    new_request->request_type = type;
    new_request->flags        = flags;
    new_request->status       = PENDING;
    new_request->fd           = fd;
    new_request->buffer_size  = buffer_size;

    if (type == WRITE) {
        strncpy(new_request->buf, buf, sizeof(new_request->buf));
        DEBUG_FS_TASK("[FS_TASK][ADD_REQUEST]: buf: %s and buf length: %d\n",
            new_request->buf, buffer_size);
    }

    if (type == OPEN || type == FIND || type == CREATE) {
        strncpy(new_request->path, path, sizeof(new_request->path));
        DEBUG_FS_TASK("[FS_TASK][ADD_REQUEST]: path: %s and buf length: %d\n",
            new_request->path, buffer_size);
    }

    request_queue[request_queue_count] = new_request;
    request_queue_count++;

    DEBUG_FS_TASK("[FS_TASK][ADD_REQUEST]: request added\n");
    DEBUG_FS_TASK("[FS_TASK][ADD_REQUEST]: caller pid: %d\n", new_request->caller_pid);
    DEBUG_FS_TASK("[FS_TASK][ADD_REQUEST]: request_type: %d\n", new_request->request_type);
    DEBUG_FS_TASK("[FS_TASK][ADD_REQUEST]: fd: %d\n", new_request->fd);
    DEBUG_FS_TASK("[FS_TASK][ADD_REQUEST]: buffer length : %d\n", new_request->buffer_size);
    DEBUG_FS_TASK("[FS_TASK][ADD_REQUEST]: flags: %d\n", new_request->flags);

    scheduler_wake_task(fs_task_pid);
    return STATUS_OK;

case_error:
    scheduler_wake_task(caller_pid);
    return STATUS_ERROR;
}

/**
 * Collect request - Should a task want to collect results.
 * @pid: Callers pid to match with request pid
 * @buf: on read situation. Read the contents of a request to buffer

 * Context: It was made so that caller could do other things and be notified to
 * when to collect. Return: the type of request or STATUS_ERROR
 */
int fs_collect_req(uint32_t caller_pid, char *out) {

    for (int i = 0; i < MAX_REQ_ENTRIES; i++) {
        if (request_queue[i] != NULL &&
            request_queue[i]->caller_pid == caller_pid &&
            request_queue[i]->status == COMPLETE) {

            switch (request_queue[i]->request_type) {
            case OPEN:
                request_queue[i]->status = TERMINATED;
                return request_queue[i]->fd;
            case READ:
                memcpy(out, request_queue[i]->buf,
                    request_queue[i]->buffer_size);
            }
            request_queue[i]->status = TERMINATED;
            return STATUS_OK;
        }
    }
    return STATUS_ERROR;
}