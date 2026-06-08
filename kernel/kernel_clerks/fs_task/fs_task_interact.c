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
int add_request_to_queue(uint32_t pid, operations_t type, uint32_t fd,
    const char *path, const char *buf,
    uint32_t buffer_size, uint32_t flags) {
    // DEBUG("[FS_TASK][ADD_REQUEST]: adding a request for fs_task\n");

    request_queue_t *new_request =
        (request_queue_t *)kmalloc(sizeof(request_queue_t));
    if (new_request == NULL) {
        ERROR("[FS_TASK][ADD_REQUEST]: could on allocate new requestat this "
              "time. Aborting\n");
        fs_wake_task(pid);
        return STATUS_ERROR;
    }

    if ((fd < 2 || fd > MAX_FD_ENTRIES) && (type != OPEN && type != CREATE)) {
        ERROR("[FS_TASK][ADD_REQUEST]: Invalid fd number. Aborting\n");
        fs_wake_task(pid);
        return STATUS_ERROR;
    }

    new_request->caller_pid   = pid;
    new_request->request_type = type;
    new_request->flags        = flags;

    switch (new_request->request_type) {
    case CREATE:
    case OPEN:
        strncpy(new_request->path, path,
            sizeof(new_request->path) - 1); // copy the path string to path
        new_request->path[sizeof(new_request->path) - 1] =
            '\0'; // end it ate 127. path is 128 long
        DEBUG("[FS_TASK][ADD_REQUEST]: path: %s\n", new_request->path);
        break;
    case WRITE:
        strncpy(new_request->buf, buf, sizeof(new_request->buf));
        new_request->fd          = fd;
        new_request->buffer_size = buffer_size;
        // DEBUG("[FS_TASK][ADD_REQUEST]: buf: %s and buf length: %d\n",
        //      new_request->buf, buffer_size);
        break;
    case CLOSE:
        new_request->fd = fd;
        break;
    case READ:
        new_request->fd          = fd;
        new_request->buffer_size = buffer_size;
        break;
    default:
        break;
    }

    /*
    DEBUG("[FS_TASK][ADD_REQUEST]: pid: %d\n", new_request->caller_pid);
    DEBUG("[FS_TASK][ADD_REQUEST]: request_type: %d\n",
    new_request->request_type); DEBUG("[FS_TASK][ADD_REQUEST]: fd:
    %d\n",new_request->fd); DEBUG("[FS_TASK][ADD_REQUEST]: buffer length: %d\n",
    new_request->buffer_size); DEBUG("[FS_TASK][ADD_REQUEST]: flags:
    %d\n",new_request->flags);
    */

    new_request->status                = PENDING;
    request_queue[request_queue_count] = new_request;
    request_queue_count++;

    // DEBUG("[FS_TASK][ADD_REQUEST]: request added\n");
    task_t *fs_task = task_get(fs_task_pid);
    fs_task->priority =
        PRIORITY_HIGH;           // set fs_task to be high so it is picked more frequently
    fs_task->state = TASK_READY; // set it ready so it can be picked at all

    return STATUS_OK;
}

/**
 * Collect request - Should a task want to collect results.
 * @pid: Callers pid to match with request pid
 * @buf: on read situation. Read the contents of a request to buffer
 *
 * Description:
 * more lenghty desription on what the function DOES
 *
 * Context: It was made so that caller could do other things and be notified to
 * when to collect. Return: the type of request or STATUS_ERROR
 */
int collect_request(uint32_t pid, char *out) {
    // DEBUG("[FS_TASK][COLLECT_REQUEST]: Fetching request for %d\n", pid);

    for (int i = 0; i < request_queue_count; i++) {
        if (request_queue[i] != NULL && request_queue[i]->caller_pid == pid &&
            request_queue[i]->status == COMPLETE) {
            // DEBUG("[FS_TASK][COLLECT_REQUEST]: Request found!\n");
            switch (request_queue[i]->request_type) {
            case OPEN:
                // DEBUG("[FS_TASK][COLLECT_REQUEST]: Returning fd: %d\n",
                // request_queue[i]->fd);
                request_queue[i]->status = TERMINATED;
                return request_queue[i]->fd;

            case READ:
                // DEBUG("[FS_TASK][COLLECT_REQUEST]: Returning read file to
                // %d\n",
                //       pid);
                // DEBUG("[FS_TASK][COLLECT_REQUEST]: File contents %s\n",
                //      request_queue[i]->buf);
                memcpy(out, request_queue[i]->buf,
                    request_queue[i]->buffer_size);
                request_queue[i]->status = TERMINATED;
                return STATUS_OK;

            case WRITE:
                // DEBUG("[FS_TASK][COLLECT_REQUEST]: Returning new offset for:
                // %d\n", pid);
                fd_entry_t *entry        = fd_entry_table[request_queue[i]->fd];
                request_queue[i]->status = TERMINATED;
                return entry->curr_offset;
            case CREATE:
                return STATUS_OK;
            case DELETE:
                return STATUS_OK;

            default:
                break;
            }
        }
    }
    // DEBUG("[FS_TASK][COLLECT_REQUEST]: Unable to fetch request.\n");
    return STATUS_ERROR;
}