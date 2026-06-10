#include "ledger.h"
#include "blankie.h"

request_table *request_queue[CLERK_COUNT][MAX_REQ_ENTRIES];

/*
 * Ledger Protocol
 * Design & Implementation: A.H, 2026
 */

static void wake_clerk(uint32_t clerk_pid) {
    scheduler_wake_task(clerk_pid);
}

/**
 * add_request_to_ledger - makes a new entry req.
 * @param1: short description.
 * @param2: short desription.
 *
 * Description:
 * This function validetae the params given to it
 * and after that makes a new entry to request_table
 *
 * Context: Why was it made, when to call it.
 * Return: what if successfull, what if unsuffessfull.
 */
int add_request_to_ledger(uint32_t caller_pid, uint32_t clerk_pid,
    operations_t type, uint32_t fd, const char *path,
    const char *buf, uint32_t buffer_size, uint32_t flags) {

    if (clerk_pid >= CLERK_COUNT) {
        ERROR("[LEDGER][ADD_REQUEST]: clerk pid is invalid\n");
        goto case_error;
    }

    request_table *new_request = (request_table *)kmalloc(sizeof(request_table));
    if (new_request == NULL) {
        ERROR("[LEDGER][ADD_REQUEST]: could not allocate new request. Aborting\n");
        goto case_error;
    }

    if ((fd < 2 || fd > MAX_FD_ENTRIES) && (type != OPEN && type != CREATE && type != FIND)) {
        ERROR("[LEDGER][ADD_REQUEST]: Invalid fd number. Aborting\n");
        goto case_error;
    }

    new_request->caller_pid   = caller_pid;
    new_request->clerk_pid    = clerk_pid;
    new_request->request_type = type;
    new_request->flags        = flags;
    new_request->status       = PENDING;
    new_request->fd           = fd;
    new_request->buffer_size  = buffer_size;

    if (type == WRITE) {
        strncpy(new_request->buf, buf, sizeof(new_request->buf));
        DEBUG("[LEDGER][ADD_REQUEST]: buf: %s and buf length: %d\n",
            new_request->buf, buffer_size);
    }

    if (type == OPEN || type == FIND || type == CREATE) {
        strncpy(new_request->path, path, sizeof(new_request->path));
        DEBUG("[LEDGER][ADD_REQUEST]: path: %s and buf length: %d\n",
            new_request->path, buffer_size);
    }

    int found = -1;
    for (int i = 0; i < MAX_REQ_ENTRIES; i++) {
        if (request_queue[clerk_pid][i] == NULL) {
            request_queue[clerk_pid][i] = new_request;
            found                       = 1;
            break;
        }
    }

    if (found == -1)
        goto case_error;

    DEBUG("[LEDGER][ADD_REQUEST]: request added\n");
    DEBUG("[LEDGER][ADD_REQUEST]: caller pid: %d\n", new_request->caller_pid);
    DEBUG("[LEDGER][ADD_REQUEST]: clerkpid: %d\n", new_request->clerk_pid);
    DEBUG("[LEDGER][ADD_REQUEST]: request_type: %d\n", new_request->request_type);
    DEBUG("[LEDGER][ADD_REQUEST]: fd: %d\n", new_request->fd);
    DEBUG("[LEDGER][ADD_REQUEST]: buffer length : %d\n", new_request->buffer_size);
    DEBUG("[LEDGER][ADD_REQUEST]: flags: %d\n", new_request->flags);

    wake_clerk(clerk_pid);
    return STATUS_OK;

case_error:
    scheduler_wake_task(caller_pid);
    return STATUS_ERROR;
}

int collect_request(uint32_t caller_pid, uint32_t clerk_pid, char *out) {
    if (clerk_pid >= CLERK_COUNT) {
        ERROR("[LEDGER][COLLECT]: clerk pid is invalid\n");
        return STATUS_ERROR;
    }

    for (int i = 0; i < MAX_REQ_ENTRIES; i++) {
        if (request_queue[clerk_pid][i] != NULL &&
            request_queue[clerk_pid][i]->caller_pid == caller_pid &&
            request_queue[clerk_pid][i]->status == COMPLETE) {

            switch (request_queue[clerk_pid][i]->request_type) {
            case OPEN:
                request_queue[clerk_pid][i]->status = TERMINATED;
                return request_queue[clerk_pid][i]->fd;

            case READ:
                memcpy(out, request_queue[clerk_pid][i]->buf,
                    request_queue[clerk_pid][i]->buffer_size);

                request_queue[clerk_pid][i]->status = TERMINATED;
                return STATUS_OK;

            case WRITE:
            case CREATE:
            case DELETE:
            case FIND:
                request_queue[clerk_pid][i]->status = TERMINATED;
                return STATUS_OK;

            default:
                break;
            }
        }
    }
    return STATUS_ERROR;
}

request_table *fetch_next_task(uint32_t clerk_pid) {

    DEBUG("[LEDGER][FETCH_NEXT_TASK]: %d came to look for a taks!\n", clerk_pid);

    if (clerk_pid >= CLERK_COUNT) {
        ERROR("[LEDGER][FETCH_NEXT_TASK]: clerk pid is invalid\n");
        return NULL;
    }

    for (int i = 0; i < MAX_REQ_ENTRIES; i++) {
        if (request_queue[clerk_pid][i] != NULL &&
            request_queue[clerk_pid][i]->status == IN_PROGRESS) {
            DEBUG("[LEDGER][FETCH_NEXT_TASK]: IN_PROGRESS_FOUND\n");
            return request_queue[clerk_pid][i];
        }
    }

    for (int i = 0; i < MAX_REQ_ENTRIES; i++) {
        if (request_queue[clerk_pid][i] != NULL &&
            request_queue[clerk_pid][i]->status == PENDING) {
            DEBUG("[LEDGER][FETCH_NEXT_TASK]: PENDING_FOUND\n");
            return request_queue[clerk_pid][i];
        }
    }

    DEBUG("[LEDGER][NEXT_REQUEST]: could not find new request\n");
    return NULL;
}

void ledger_init() {
    DEBUG("[LEDGER][INIT]: \n");
    memset(request_queue, 0, sizeof(request_queue));
}