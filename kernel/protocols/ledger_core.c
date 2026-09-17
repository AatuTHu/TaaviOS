#include "config.h"
#include "klog.h"
#include "kmalloc.h"
#include "kstring.h"
#include "ledger.h"
#include "sched.h"
#include "shared.h"
#include <stdint.h>
/*
 * Ledger Protocol
 * Design & Implementation: A.H, 2026
 */

request_table *fs_table[MAX_FS_REQ_ENTRIES];
int last_fs_req_idx = -1;

request_table *gui_table[MAX_GUI_REQ_ENTRIES];
int last_gui_req_idx = -1;

request_table *reaper_table[MAX_REAPER_REQ_ENTRIES];
int last_reaper_req_idx               = -1;

clerk_queue clerk_queues[CLERK_COUNT] = {
    [fs_task_pid]     = {fs_table, MAX_FS_REQ_ENTRIES, &last_fs_req_idx},
    [gui_task_pid]    = {gui_table, MAX_GUI_REQ_ENTRIES, &last_gui_req_idx},
    [reaper_task_pid] = {reaper_table, MAX_REAPER_REQ_ENTRIES, &last_reaper_req_idx},
};

static void wake_clerk(uint32_t clerk_pid) {
    task_t *clerk = task_get(clerk_pid);
    if (clerk != NULL && clerk_pid != reaper_task_pid) {
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
 * the faulty request as terminated and wakes the caller.
 *
 */
void ledger_check_request(uint32_t clerk_pid) {
    clerk_queue *q = ledger_get_queue(clerk_pid);
    if (q == NULL || *q->last_idx == -1) {
        return;
    }

    request_table *entry = q->table[*q->last_idx];
    if (entry != NULL) {
        ERROR("[LEDGER][CHECK_REQUEST]: force terminating last request and waking caller\n");
        entry->status = TERMINATED;
        scheduler_wake_task(entry->caller_pid);
    }
}

void ledger_remove_request() {

    for (int c = 0; c < CLERK_COUNT; c++) {
        clerk_queue *q = ledger_get_queue(c);
        if (q == NULL || q->table == NULL) {
            continue;
        }

        for (int i = 0; i < q->max_entries; i++) {
            if (q->table[i] != NULL && q->table[i]->status == TERMINATED) {

                if (q->table[i]->pixels != NULL) {
                    kfree(q->table[i]->pixels);
                    q->table[i]->pixels = NULL;
                }

                if (q->table[i]->buf != NULL) {
                    kfree(q->table[i]->buf);
                    q->table[i]->buf = NULL;
                }

                kfree(q->table[i]);
                q->table[i] = NULL;
                //  DEBUG_LEDGER("[LEDGER][REMOVE]: Reaper removed one request\n");
            }
        }
    }
}

static inline int queue_req(request_table *new_request, uint32_t clerk_pid, uint32_t caller_pid) {

    clerk_queue *q = ledger_get_queue(clerk_pid);
    if (!q) {
        kfree(new_request);
        scheduler_wake_task(caller_pid);
        return STATUS_ERROR;
    }

    for (int i = 0; i < q->max_entries; i++) {
        if (q->table[i] == NULL) {
            q->table[i] = new_request;
            wake_clerk(clerk_pid);
            return STATUS_OK;
        }
    }

    kfree(new_request);
    scheduler_wake_task(caller_pid);
    return STATUS_ERROR;
}

static char *pack_dimensions(uint32_t value, char *buf) {
    char tmp[10];
    int i = 0;

    if (value == 0) {
        *buf++ = '0';
        return buf;
    }

    while (value > 0) {
        tmp[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        *buf++ = tmp[--i];
    }

    return buf;
}

/**
 * ledger_collect - retrieves a COMPLETE request belonging to caller_pid.
 * @caller_pid: pid of the task collecting its result
 * @clerk_pid:  pid of the clerk whose queue should be searched
 * @out:        buffer to copy READ results into
 *
 * Description:
 * Scans the clerk's queue for a COMPLETE request owned by caller_pid.
 * OPEN requests return the allocated FD directly. READ requests copy
 * their buffer into out. All other types fall through to the shared
 * completion path. Any matching TERMINATED entries also trigger the
 * reaper so stale requests don't linger.
 *
 * Return: STATUS_OK / FD on success / buffer containing width and height
 * STATUS_ERROR if nothing found.
 */
int ledger_collect(uint32_t caller_pid, uint32_t clerk_pid, char *out) {
    clerk_queue *q = ledger_get_queue(clerk_pid);
    // DEBUG_LEDGER("[LEDGER][COLLECT]: %d is collecting %d request\n", caller_pid, clerk_pid);
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
            case CREATE:
            case OPEN:
                req->status = TERMINATED;
                DEBUG_LEDGER("[LEDGER][COLLECT]: collecting struct_key: %d\n", req->struct_key);
                return req->struct_key;
            case LIST:
            case READ:
                if (out != NULL) {
                    memcpy(out, req->buf, req->buffer_size);
                    out[req->buffer_size] = '\0';
                    // DEBUG_LEDGER("[LEDGER][COLLECT]: %d is collecting to a buffer the size of %d containing: %s\n", caller_pid, req->buffer_size, req->buf);
                }
                req->status = TERMINATED;
                return req->buffer_size;
            case RESIZE:
                if (out != NULL) {
                    DEBUG_LEDGER("[LEDGER][COLLECT]: %d is collecting width and height\n", caller_pid);
                    char *params = out;

                    params       = pack_dimensions(req->width, params);
                    *params++    = '.';
                    params       = pack_dimensions(req->height, params);
                    *params      = '\0';
                }
                // DEBUG_LEDGER("[LEDGER][COLLECT]: params packed to go %s\n", out);
                req->status = TERMINATED;
                return STATUS_OK;

            default:
                break;
            }

            req->status = TERMINATED;
            return STATUS_OK;
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
            return q->table[i];
        }
    }

    for (int i = 0; i < q->max_entries; i++) {
        if (q->table[i] != NULL && q->table[i]->status == PENDING) {
            *q->last_idx        = i;
            q->table[i]->status = IN_PROGRESS;
            return q->table[i];
        }
    }

    // DEBUG_LEDGER("[LEDGER][FETCH_NEXT_TASK]: No tasks found for :%d\n", clerk_pid);
    return NULL;
}

int ledger_count_clerk_reqs(uint32_t clerk_pid) {

    //    DEBUG_LEDGER("[LEDGER][COUNT_CLERK_REQS]: Counting for %d\n", clerk_pid);

    if (clerk_pid >= CLERK_COUNT) {
        return STATUS_ERROR;
    }

    const clerk_queue *q = ledger_get_queue(clerk_pid);
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

    return req_count;
}

int ledger_count_active_reqs() {
    int req_count = 0;
    for (uint32_t clerk_pid = 0; clerk_pid < CLERK_COUNT; clerk_pid++) {
        const clerk_queue *q = ledger_get_queue(clerk_pid);
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

int ledger_has_killable_reqs() {
    for (uint32_t clerk_pid = 0; clerk_pid < CLERK_COUNT; clerk_pid++) {
        const clerk_queue *q = ledger_get_queue(clerk_pid);
        if (q == NULL) {
            continue;
        }
        for (int i = 0; i < q->max_entries; i++) {
            if (q->table[i] != NULL && q->table[i]->status == TERMINATED) {
                return 1;
            }
        }
    }

    return 0;
}

int ledger_queue_free_req(uint32_t caller_pid, uint32_t clerk_pid, uint32_t target_pid) {
    if (caller_pid >= MAX_TASKS || target_pid >= MAX_TASKS || target_pid < CLERK_COUNT) {
        ERROR("[LEDGER][ADD_REAPER_REQUEST]: Callers pid or target pid was invalid. Aborting\n");
        return STATUS_ERROR;
    }

    request_table *new_request = (request_table *)kmalloc(sizeof(request_table));

    if (new_request == NULL) {
        ERROR("[LEDGER][FREE_REQ]: Could not allocate memory for the request\n");
        return STATUS_ERROR;
    }

    memset(new_request, 0, sizeof(request_table));
    new_request->caller_pid   = caller_pid;
    new_request->target_pid   = target_pid;
    new_request->request_type = FREE;
    new_request->clerk_pid    = clerk_pid;

    return queue_req(new_request, clerk_pid, caller_pid);
}

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

    if (buffer_size > 0 && buf != NULL) {
        new_request->buf = (char *)kmalloc(buffer_size + 1);
        if (new_request->buf == NULL) {
            ERROR("Could not allocate buffer for the message\n");
            kfree(new_request);
            return STATUS_ERROR;
        }
        memcpy(new_request->buf, buf, buffer_size);
        new_request->buffer_size      = buffer_size;
        new_request->buf[buffer_size] = '\0';
        DEBUG_FS_TASK("[LEDGER][ADD_FS_REQUEST] buffer: %s\n", new_request->buf);
    }

    // DEBUG_FS_TASK("[LEDGER][ADD_FS_REQUEST]: caller pid: %d\n", new_request->caller_pid);
    // DEBUG_FS_TASK("[LEDGER][ADD_FS_REQUEST]: clerk_pid: %d\n", new_request->clerk_pid);
    // DEBUG_FS_TASK("[LEDGER][ADD_FS_REQUEST]: request_type: %d\n", new_request->request_type);
    // DEBUG_FS_TASK("[LEDGER][ADD_FS_REQUEST]: fd: %d\n", new_request->struct_key);
    // DEBUG_FS_TASK("[LEDGER][ADD_FS_REQUEST]: buffer length : %d\n", new_request->buffer_size);
    // DEBUG_FS_TASK("[LEDGER][ADD_FS_REQUEST]: flags: %d\n", new_request->flags);

    return queue_req(new_request, fs_task_pid, caller_pid);

case_error:
    scheduler_wake_task(caller_pid);
    return STATUS_ERROR;
}

/**
 * ledger_add_gui_req - makes a new entry req.
 * @params: paced set of params used to make the request
 *
 * Description:
 * Validates paramss, allocates a new request_table entry, places it in
 * the correct clerk's queue and wakes that clerk.
 *
 * Return: STATUS_OK on success, STATUS_ERROR on failure.
 */
int ledger_add_gui_req(uint32_t caller_pid, const gui_params_pack *params) {

    request_table *new_request = (request_table *)kmalloc(sizeof(request_table));
    if (new_request == NULL) {
        ERROR("[LEDGER][ADD_GUI_REQUEST]: could not allocate new request. Aborting\n");
        goto case_error;
    }

    memset(new_request, 0, sizeof(request_table));
    new_request->caller_pid   = caller_pid;
    new_request->struct_key   = params->struct_key;
    new_request->clerk_pid    = gui_task_pid;
    new_request->target_pid   = caller_pid;
    new_request->request_type = params->opcode;
    new_request->status       = PENDING;
    new_request->width        = params->width;
    new_request->height       = params->height;
    new_request->x            = params->x;
    new_request->y            = params->y;
    new_request->scale        = params->scale;
    new_request->fg_color     = params->fg_color;
    new_request->bg_color     = params->bg_color;

    if (params->pixels != NULL) {
        size_t pixel_bytes  = params->width * params->height * sizeof(uint32_t);
        new_request->pixels = (uint32_t *)kmalloc(pixel_bytes);

        if (new_request->pixels == NULL) {
            kfree(new_request);
            goto case_error;
        }

        memcpy(new_request->pixels, params->pixels, pixel_bytes);
    }

    if (params->buf != NULL) {
        new_request->buf = (char *)kmalloc(params->buffer_size + 1);
        if (new_request->buf != NULL) {
            new_request->buffer_size = params->buffer_size;
            memcpy(new_request->buf, params->buf, params->buffer_size);
            new_request->buf[params->buffer_size] = '\0';
        }
    }

    // DEBUG_GUI_TASK("[LEDGER][ADD_GUI_REQUEST]: caller pid: %d\n", new_request->caller_pid);
    // DEBUG_GUI_TASK("[LEDGER][ADD_GUI_REQUEST]: clerkpid: %d\n", new_request->clerk_pid);
    // DEBUG_GUI_TASK("[LEDGER][ADD_GUI_REQUEST]: request_type: %d\n", new_request->request_type);
    // DEBUG_GUI_TASK("[LEDGER][ADD_GUI_REQUEST]: width: %d\n", new_request->width);
    // DEBUG_GUI_TASK("[LEDGER][ADD_GUI_REQUEST]: height: %d\n", new_request->height);
    // DEBUG_GUI_TASK("[LEDGER][ADD_GUI_REQUEST]: x: %d\n", new_request->x);
    // DEBUG_GUI_TASK("[LEDGER][ADD_GUI_REQUEST]: y: %d\n", new_request->y);
    //  DEBUG_GUI_TASK("[LEDGER][ADD_GUI_REQUEST]: buffer length : %d\n", new_request->buffer_size);

    return queue_req(new_request, gui_task_pid, caller_pid);

case_error:
    scheduler_wake_task(caller_pid);
    return STATUS_ERROR;
}

void ledger_init() {
    for (int i = 0; i < MAX_FS_REQ_ENTRIES; i++) {
        fs_table[i] = NULL;
    }
    for (int i = 0; i < MAX_GUI_REQ_ENTRIES; i++) {
        gui_table[i] = NULL;
    }

    for (int i = 0; i < MAX_REAPER_REQ_ENTRIES; i++) {
        reaper_table[i] = NULL;
    }

    last_gui_req_idx    = -1;
    last_fs_req_idx     = -1;
    last_reaper_req_idx = -1;
}
