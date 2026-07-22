#include "blankie.h"
#include "ledger.h"

/*
 * Ledger Protocol
 * Design & Implementation: A.H, 2026
 */

request_table *fs_table[MAX_FS_REQ_ENTRIES];
int last_fs_req_idx = -1;

request_table *gui_table[MAX_GUI_REQ_ENTRIES];
int last_gui_req_idx                  = -1;

clerk_queue clerk_queues[CLERK_COUNT] = {
    [fs_task_pid]  = {fs_table, MAX_FS_REQ_ENTRIES, &last_fs_req_idx},
    [gui_task_pid] = {gui_table, MAX_GUI_REQ_ENTRIES, &last_gui_req_idx},
};

static void wake_clerk(uint32_t clerk_pid) {
    task_t *clerk = task_get_by_pid(clerk_pid);
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

    if (clerk_pid >= CLERK_COUNT) {
        return NULL;
    }

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

                if (q->table[i]->pixels != NULL) {
                    kfree(q->table[i]->pixels);
                }

                kfree(q->table[i]);
                q->table[i] = NULL;
                kill_count++;
            }
        }
    }

    DEBUG_LEDGER("[LEDGER][REMOVE]: Reaper exiting with kill count of %d\n", kill_count);
    return kill_count;
}

int ledger_enqueue(uint32_t clerk_pid, request_table *req) {
    clerk_queue *q = ledger_get_queue(clerk_pid);
    if (!q)
        return STATUS_ERROR;

    for (int i = 0; i < q->max_entries; i++) {
        if (q->table[i] == NULL) {
            q->table[i] = req;
            wake_clerk(clerk_pid);
            return STATUS_OK;
        }
    }
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

                return req->fd;
            case LIST:
                if (out != NULL) {
                    DEBUG_LEDGER("[LEDGER][COLLECT]: %d is collecting to a buffer the size of %d\n", caller_pid, req->buffer_size);
                    memcpy(out, req->buf, req->buffer_size);
                }
                req->status = TERMINATED;
                return req->buffer_size;
            case READ:
                if (out != NULL) {
                    memcpy(out, req->buf, req->buffer_size);
                }
                break;
            default:
                break;
            }

            req->status = TERMINATED;
            DEBUG_LEDGER("[LEDGER][COLLECT]: Collectables found for: %d. With req.type: %d\n", caller_pid, req->request_type);
            return STATUS_OK;
        }

        if (req->status == TERMINATED) {
            wake_clerk(reaper_task_pid);
        }
    }

    DEBUG_LEDGER("[LEDGER][COLLECT]: Could not find any collectable requests for %d\n", caller_pid);
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
    if (clerk_pid >= CLERK_COUNT) {
        DEBUG_LEDGER("[LEDGER][FETCH_NEXT_TASK]: Invalid clerk pid!\n");
        return NULL;
    }

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

    for (int i = 0; i < q->max_entries; i++) {
        if (q->table[i] != NULL && q->table[i]->status == FAILED) {
            *q->last_idx = i;
            DEBUG_LEDGER("[LEDGER][FETCH_NEXT_TASK]: PENDING_FOUND AT %d\n", i);
            q->table[i]->status = IN_PROGRESS;
            return q->table[i];
        }
    }

    DEBUG_LEDGER("[LEDGER][FETCH_NEXT_TASK]: No tasks found for :%d\n", clerk_pid);
    return NULL;
}

int ledger_count_clerk_reqs(uint32_t clerk_pid) {

    //    DEBUG_LEDGER("[LEDGER][COUNT_CLERK_REQS]: Counting for %d\n", clerk_pid);

    if (clerk_pid > CLERK_COUNT) {
        return STATUS_ERROR;
    }

    clerk_queue *q = ledger_get_queue(clerk_pid);
    if (q == NULL) {
        ERROR("[LEDGER][CONUT CLERKS]: clerk pid is invalid\n");
        return 0;
    }

    int req_count = 0;

    for (int i = 0; i < q->max_entries; i++) {
        if (q->table[i] != NULL && (q->table[i]->status == PENDING || q->table[i]->status == IN_PROGRESS)) {
            req_count++;
        }
    }
    //  DEBUG_LEDGER("[LEDGER][COUNT_CLERK_REQS]: Found %d requests\n", req_count);
    return req_count;
}

int ledger_count_active_reqs() {
    int req_count = 0;
    for (uint32_t clerk_pid = 0; clerk_pid < CLERK_COUNT; clerk_pid++) {
        clerk_queue *q = ledger_get_queue(clerk_pid);
        if (q == NULL) {
            continue;
        }
        for (int i = 0; i < q->max_entries; i++) {
            if (q->table[i] != NULL && (q->table[i]->status == PENDING || q->table[i]->status == IN_PROGRESS)) {
                req_count++;
            }
        }
    }

    return req_count;
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