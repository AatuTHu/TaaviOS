#include "ledger.h"
#include "blankie.h"

/*
 * Ledger Protocol
 * Design & Implementation: A.H, 2026
 */

request_table *fs_table[MAX_FS_REQ_ENTRIES];
int last_fs_req_idx = -1;

request_table *gui_table[MAX_GUI_REQ_ENTRIES];
int last_gui_req_idx = -1;

clerk_queue clerk_queues[CLERK_COUNT] = {
    [fs_task_pid]  = {fs_table, MAX_FS_REQ_ENTRIES, &last_fs_req_idx},
    [gui_task_pid] = {gui_table, MAX_GUI_REQ_ENTRIES, &last_gui_req_idx},
};

static void wake_clerk(uint32_t clerk_pid) {
    task_t *clerk = task_get(clerk_pid);
    if (clerk_pid != reaper_task_pid) {
        clerk->priority = PRIORITY_HIGH;
    }
    scheduler_wake_task(clerk_pid);
}

/**
 * ledger_get_queue - resolves the clerk_queue_t for a given clerk pid.
 * @clerk_pid: pid of the clerk
 *
 * Description:
 * Single point of validation for clerk_pid -> queue lookups. Replaces
 * the per-function switch statements. Returns NULL if the pid has no
 * registered queue (invalid pid, or unused slot in clerk_queues).
 *
 * Return: pointer to the clerk's queue, or NULL if invalid.
 */
static clerk_queue *ledger_get_queue(uint32_t clerk_pid) {

    clerk_queue *q = &clerk_queues[clerk_pid];
    if (q->table == NULL) {
        return NULL;
    }

    return q;
}

/**
 * ledger_check_request - Marks the last request as terminated.
 * @clerk_pid: pid of the clerk
 *
 * Description:
 * In case of fault this function is called clerks recovery function. It marks
 * the faulty request as termianted and wakes the caller.
 *
 */
void ledger_check_request(uint32_t clerk_pid) {
    clerk_queue *q = ledger_get_queue(clerk_pid);
    if (q == NULL || *q->last_idx == -1) {
        return;
    }

    request_table *entry = q->table[*q->last_idx];
    if (entry != NULL) {
        entry->status = TERMINATED;
        scheduler_wake_task(entry->caller_pid);
    }
}

int ledger_remove_request() {
    // DEBUG_LEDGER("[LEDGER][REMOVE]: Dont fear the reaper\n");
    int kill_count = 0;

    for (int c = 0; c < CLERK_COUNT; c++) {
        clerk_queue *q = ledger_get_queue(c);
        if (q == NULL) {
            continue;
        }

        for (int i = 0; i < q->max_entries; i++) {
            if (q->table[i] != NULL && q->table[i]->status == TERMINATED) {
                DEBUG_LEDGER("[LEDGER][REMOVE]: Reaper came to reap clerk %d, slot %d\n", c, i);
                kfree(q->table[i]);
                q->table[i] = NULL;
                kill_count++;
            }
        }
    }

    DEBUG_LEDGER("[LEDGER][REMOVE]: Reaper exiting with kill count of %d\n", kill_count);
    return kill_count;
}

/**
 * ledger_add_req - makes a new entry req.
 * @caller_pid:  pid of the task making the request
 * @clerk_pid:   pid of the clerk that should service the request
 * @type:        operation type (READ, WRITE, OPEN, ...)
 * @fd:          file descriptor (ignored for OPEN/CREATE/FIND)
 * @path:        path string, used for OPEN/CREATE/FIND
 * @buf:         data buffer, used for WRITE
 * @buffer_size: size of buf / requested size
 * @flags:       open/access flags
 *
 * Description:
 * Validates params, allocates a new request_table entry, places it in
 * the correct clerk's queue and wakes that clerk.
 *
 * Return: STATUS_OK on success, STATUS_ERROR on failure.
 */
int ledger_add_req(uint32_t caller_pid, uint32_t clerk_pid,
    operations_t type, uint32_t fd, const char *path,
    const char *buf, uint32_t buffer_size, uint32_t flags) {

    clerk_queue *q = ledger_get_queue(clerk_pid);
    if (q == NULL) {
        ERROR("[LEDGER][ADD_REQUEST]: clerk pid is invalid\n");
        goto case_error;
    }

    request_table *new_request = (request_table *)kmalloc(sizeof(request_table));
    if (new_request == NULL) {
        ERROR("[LEDGER][ADD_REQUEST]: could not allocate new request. Aborting\n");
        goto case_error;
    }

    if (clerk_pid == fs_task_pid) {
        if ((fd < 2 || fd > MAX_FD_ENTRIES) && (type == READ || type == WRITE)) {
            ERROR("[LEDGER][ADD_REQUEST]: Invalid fd number. Aborting\n");
            kfree(new_request);
            goto case_error;
        }
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
        // DEBUG_LEDGER("[LEDGER][ADD_REQUEST]: buf: %s and buf length: %d\n",
        //     new_request->buf, buffer_size);
    }

    if ((type == OPEN || type == FIND || type == CREATE) && clerk_pid == fs_task_pid) {
        strncpy(new_request->path, path, sizeof(new_request->path));
        // DEBUG_LEDGER("[LEDGER][ADD_REQUEST]: path: %s and buf length: %d\n",
        //     new_request->path, buffer_size);
    }

    int found = -1;
    for (int i = 0; i < q->max_entries; i++) {
        if (q->table[i] == NULL) {
            q->table[i] = new_request;
            found       = i;
            break;
        }
    }

    if (found == -1) {
        ERROR("[LEDGER][ADD_REQUEST]: No free spots in the table\n");
        kfree(new_request);
        goto case_error;
    }

    DEBUG_LEDGER("[LEDGER][ADD_REQUEST]: request added to index %d\n", found);
    DEBUG_LEDGER("[LEDGER][ADD_REQUEST]: caller pid: %d\n", new_request->caller_pid);
    DEBUG_LEDGER("[LEDGER][ADD_REQUEST]: clerkpid: %d\n", clerk_pid);
    DEBUG_LEDGER("[LEDGER][ADD_REQUEST]: request_type: %d\n", new_request->request_type);
    DEBUG_LEDGER("[LEDGER][ADD_REQUEST]: fd: %d\n", new_request->fd);
    DEBUG_LEDGER("[LEDGER][ADD_REQUEST]: buffer length : %d\n", new_request->buffer_size);
    DEBUG_LEDGER("[LEDGER][ADD_REQUEST]: flags: %d\n", new_request->flags);

    wake_clerk(clerk_pid);
    return STATUS_OK;

case_error:
    scheduler_wake_task(caller_pid);
    return STATUS_ERROR;
}

/**
 * ledger_collect - retrieves a COMPLETE request belonging to caller_pid.
 * @caller_pid: pid of the task collecting its result
 * @clerk_pid:  pid of the clerk whose queue should be searched
 * @out:        buffer to copy READ results into
 *
 * Description:
 * Scans the clerk's queue for a COMPLETE request owned by caller_pid.
 * OPEN requests return the allocated fd directly. READ requests copy
 * their buffer into out. All other types fall through to the shared
 * completion path. Any matching TERMINATED entries also trigger the
 * reaper so stale requests don't linger.
 *
 * Return: STATUS_OK / fd on success, STATUS_ERROR if nothing found.
 */
int ledger_collect(uint32_t caller_pid, uint32_t clerk_pid, char *out) {
    clerk_queue *q = ledger_get_queue(clerk_pid);
    DEBUG_LEDGER("[LEDGER][COLLECT]: %d is collecting %d request\n", caller_pid, clerk_pid);
    if (q == NULL) {
        ERROR("[LEDGER][COLLECT]: clerk pid is invalid\n");
        return STATUS_ERROR;
    }

    for (int i = 0; i < q->max_entries; i++) {
        request_table *req = q->table[i];
        if (req == NULL || req->caller_pid != caller_pid) {
            continue;
        }

        if (req->status == COMPLETE) {
            switch (req->request_type) {
            case OPEN:
                req->status = TERMINATED;
                wake_clerk(reaper_task_pid);
                return req->fd;
            case LIST:
            case READ:
                if (out != NULL) {
                    memcpy(out, req->buf, req->buffer_size);
                    out[req->buffer_size] = '\0';
                }
                break;
            default:
                break;
            }

            req->status = TERMINATED;
            wake_clerk(reaper_task_pid);
            return STATUS_OK;
        }

        if (req->status == TERMINATED) {
            wake_clerk(reaper_task_pid);
        }
    }

    return STATUS_ERROR;
}

/**
 * ledger_fetch_next_req - picks the next request for a clerk to handle.
 * @clerk_pid: pid of the clerk asking for work
 *
 * Description:
 * Searches the clerk's queue for an IN_PROGRESS request first, then
 * falls back to PENDING. Records the chosen index in the queue's
 * last_idx for use by ledger_check_request.
 *
 * Return: pointer to the next request, or NULL if none available.
 */
request_table *ledger_fetch_next_req(uint32_t clerk_pid) {
    DEBUG_LEDGER("[LEDGER][FETCH_NEXT_TASK]: %d came to look for a request!\n", clerk_pid);

    clerk_queue *q = ledger_get_queue(clerk_pid);
    if (q == NULL) {
        ERROR("[LEDGER][FETCH_NEXT_TASK]: clerk pid is invalid\n");
        return NULL;
    }

    for (int i = 0; i < q->max_entries; i++) {
        if (q->table[i] != NULL && q->table[i]->status == IN_PROGRESS) {
            *q->last_idx = i;
            DEBUG_LEDGER("[LEDGER][FETCH_NEXT_TASK]: IN_PROGRESS_FOUND\n");
            return q->table[i];
        }
    }

    for (int i = 0; i < q->max_entries; i++) {
        if (q->table[i] != NULL && q->table[i]->status == PENDING) {
            *q->last_idx = i;
            DEBUG_LEDGER("[LEDGER][FETCH_NEXT_TASK]: PENDING_FOUND AT %d\n", i);
            q->table[i]->status = IN_PROGRESS;
            return q->table[i];
        }
    }

    DEBUG_LEDGER("[LEDGER][FETCH_NEXT_TASK]: No tasks found for :%d\n", clerk_pid);
    return NULL;
}

void ledger_init() {
    for (int i = 0; i < MAX_FS_REQ_ENTRIES; i++) {
        fs_table[i] = NULL;
    }
    for (int i = 0; i < MAX_GUI_REQ_ENTRIES; i++) {
        gui_table[i] = NULL;
    }
    last_gui_req_idx = -1;
    last_fs_req_idx  = -1;
}