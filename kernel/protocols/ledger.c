#include "ledger.h"
#include "blankie.h"

request_table *fs_table[MAX_FS_REQ_ENTRIES];

static int fs_kills = 0;

/*
 * Ledger Protocol
 * Design & Implementation: A.H, 2026
 */

static void wake_clerk(uint32_t clerk_pid) {
    task_t *clerk   = scheduler_get_current_task();
    clerk->priority = PRIORITY_HIGH;
    scheduler_wake_task(clerk_pid);
}

int remove_request() {
    int kill_count = 0;
    DEBUG_LEDGER("[LEDGER][REMOVE]: Dont fear the reaper\n");
    for (int i = 0; i < MAX_FS_REQ_ENTRIES; i++) {
        if (fs_table[i] != NULL && fs_table[i]->status == TERMINATED) {
            DEBUG("[LEDGER][REMOVE]: Reaper came to reap fs_table\n");
            fs_table[i] = NULL;
            kill_count++;
        }
    }
    DEBUG_LEDGER("[LEDGER][REMOVE]: Reaper exiting with kill count of %d\n", kill_count);
    return kill_count;
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
    new_request->request_type = type;
    new_request->flags        = flags;
    new_request->status       = PENDING;
    new_request->fd           = fd;
    new_request->buffer_size  = buffer_size;

    if (type == WRITE) {
        strncpy(new_request->buf, buf, sizeof(new_request->buf));
        DEBUG_LEDGER("[LEDGER][ADD_REQUEST]: buf: %s and buf length: %d\n",
            new_request->buf, buffer_size);
    }

    if (type == OPEN || type == FIND || type == CREATE) {
        strncpy(new_request->path, path, sizeof(new_request->path));
        DEBUG_LEDGER("[LEDGER][ADD_REQUEST]: path: %s and buf length: %d\n",
            new_request->path, buffer_size);
    }

    int found = -1;
    switch (clerk_pid) {
    case fs_task_pid:
        for (int i = 0; i < MAX_FS_REQ_ENTRIES; i++) {
            if (fs_table[i] == NULL) {
                fs_table[i] = new_request;
                found       = 0;
                break;
            }
        }
        break;
    }

    DEBUG_LEDGER("[LEDGER][ADD_REQUEST]: request added\n");
    DEBUG_LEDGER("[LEDGER][ADD_REQUEST]: caller pid: %d\n", new_request->caller_pid);
    DEBUG_LEDGER("[LEDGER][ADD_REQUEST]: clerkpid: %d\n", new_request->clerk_pid);
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

int collect_request(uint32_t caller_pid, uint32_t clerk_pid, char *out) {
    if (clerk_pid >= CLERK_COUNT) {
        ERROR("[LEDGER][COLLECT]: clerk pid is invalid\n");
        return STATUS_ERROR;
    }

    switch (clerk_pid) {
    case fs_task_pid:
        for (int i = 0; i < MAX_FS_REQ_ENTRIES; i++) {
            if (fs_table[i] != NULL &&
                fs_table[i]->caller_pid == caller_pid &&
                fs_table[i]->status == COMPLETE) {

                switch (fs_table[i]->request_type) {
                case OPEN:
                    fs_table[i]->status = TERMINATED;
                    wake_clerk(reaper_task_pid);
                    return fs_table[i]->fd;
                case READ:
                    memcpy(out, fs_table[i]->buf,
                        fs_table[i]->buffer_size);
                }
                wake_clerk(reaper_task_pid);
                fs_table[i]->status = TERMINATED;
                return STATUS_OK;
            }
        }
        break;
    }

    return STATUS_ERROR;
}

request_table *fetch_next_task(uint32_t clerk_pid) {

    DEBUG_LEDGER("[LEDGER][FETCH_NEXT_TASK]: %d came to look for a request!\n", clerk_pid);

    if (clerk_pid > CLERK_COUNT) {
        ERROR("[LEDGER][FETCH_NEXT_TASK]: clerk pid is invalid\n");
        return NULL;
    }

    switch (clerk_pid) {
    case fs_task_pid:
        for (int i = 0; i < MAX_FS_REQ_ENTRIES; i++) {
            if (fs_table[i] != NULL &&
                fs_table[i]->status == IN_PROGRESS) {
                DEBUG_LEDGER("[LEDGER][FETCH_NEXT_TASK]: IN_PROGRESS_FOUND\n");
                return fs_table[i];
            }
        }

        for (int i = 0; i < MAX_FS_REQ_ENTRIES; i++) {
            if (fs_table[i] != NULL &&
                fs_table[i]->status == PENDING) {
                DEBUG_LEDGER("[LEDGER][FETCH_NEXT_TASK]: PENDING_FOUND\n");
                return fs_table[i];
            }
        }
        break;

    default:
        break;
    }

    DEBUG_LEDGER("[LEDGER][NEXT_REQUEST]: could not find new request\n");
    return NULL;
}

void ledger_init() {
    // DEBUG_LEDGER("[LEDGER][INIT]: \n");
    // memset(request_queue, 0, sizeof(request_queue));
}
