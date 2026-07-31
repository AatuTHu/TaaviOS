#include "config.h"
#include "klog.h"
#include "kmalloc.h"
#include "kstring.h"
#include "ledger.h"
#include "sched.h"
#include <stdint.h>

static inline int queue_req(request_table *new_request, uint32_t caller_pid) {
    if (ledger_enqueue(gui_task_pid, new_request) == STATUS_ERROR) {
        ERROR("[LEDGER][QUEUE_GUI_REQUEST]: Failed to add to req queue\n");
        kfree(new_request);
        scheduler_wake_task(caller_pid);
        return STATUS_ERROR;
    }
    return STATUS_OK;
}

/**
 * ledger_add_gui_req - makes a new entry req.
 * @caller_pid:  pid of the task making the request
 * @type:        operation type (WRITE)
 * @buf:         data buffer, used for WRITE
 * @buffer_size: size of buf / requested size
 *
 * Description:
 * Validates params, allocates a new request_table entry, places it in
 * the correct clerk's queue and wakes that clerk.
 *
 * Return: STATUS_OK on success, STATUS_ERROR on failure.
 */
int ledger_add_gui_req(uint32_t caller_pid, operations_t type, uint32_t width, uint32_t height,
                       uint32_t x, uint32_t y, uint32_t scale, uint32_t color,
                       const uint32_t *user_pixels) {

    request_table *new_request = (request_table *)kmalloc(sizeof(request_table));
    if (new_request == NULL) {
        ERROR("[LEDGER][ADD_GUI_REQUEST]: could not allocate new request. Aborting\n");
        goto case_error;
    }

    memset(new_request, 0, sizeof(request_table));
    new_request->caller_pid   = caller_pid;
    new_request->clerk_pid    = gui_task_pid;
    new_request->request_type = type;
    new_request->status       = PENDING;
    new_request->width        = width;
    new_request->height       = height;
    new_request->x            = x;
    new_request->y            = y;
    new_request->scale        = scale;
    new_request->color        = color;

    if (user_pixels != NULL) {
        size_t pixel_bytes  = width * height * sizeof(uint32_t);
        new_request->pixels = (uint32_t *)kmalloc(pixel_bytes);

        if (new_request->pixels == NULL) {
            kfree(new_request);
            goto case_error;
        }

        memcpy(new_request->pixels, user_pixels, pixel_bytes);
    }

    // DEBUG_GUI_TASK("[LEDGER][ADD_GUI_REQUEST]: request added\n");
    // DEBUG_LEDGER("[LEDGER][ADD_GUI_REQUEST]: caller pid: %d\n", new_request->caller_pid);
    //  DEBUG_LEDGER("[LEDGER][ADD_GUI_REQUEST]: clerkpid: %d\n", new_request->clerk_pid);
    // DEBUG_LEDGER("[LEDGER][ADD_GUI_REQUEST]: request_type: %d\n", new_request->request_type);
    //  DEBUG_LEDGER("[LEDGER][ADD_GUI_REQUEST]: width: %d\n", new_request->width);
    //  DEBUG_LEDGER("[LEDGER][ADD_GUI_REQUEST]: height: %d\n", new_request->height);
    //  DEBUG_LEDGER("[LEDGER][ADD_GUI_REQUEST]: x: %d\n", new_request->x);
    //  DEBUG_LEDGER("[LEDGER][ADD_GUI_REQUEST]: y: %d\n", new_request->y);
    //  DEBUG_LEDGER("[LEDGER][ADD_GUI_REQUEST]: buffer length : %d\n", new_request->buffer_size);

    return queue_req(new_request, caller_pid);

case_error:
    scheduler_wake_task(caller_pid);
    return STATUS_ERROR;
}

int ledger_add_gui_text(uint32_t caller_pid, uint32_t type, uint32_t x, uint32_t y, uint32_t len, const char *buf) {
    if (caller_pid > MAX_TASKS || buf == NULL || len <= 0) {
        goto case_error;
    }

    request_table *new_request = (request_table *)kmalloc(sizeof(request_table));
    if (new_request == NULL) {
        ERROR("[LEDGER][ADD_GUI_TEXT]: could not allocate new request. Aborting\n");
        goto case_error;
    }

    memset(new_request, 0, sizeof(request_table));
    new_request->caller_pid   = caller_pid;
    new_request->clerk_pid    = gui_task_pid;
    new_request->request_type = type;
    new_request->buffer_size  = len;
    new_request->x            = x;
    new_request->y            = y;
    new_request->status       = PENDING;

    if (buf != NULL) {
        new_request->buf = (char *)kmalloc(len + 1);
        if (new_request->buf != NULL) {
            new_request->buffer_size = len;
            memcpy(new_request->buf, buf, len);
            new_request->buf[len] = '\0';
        }
    }
    // DEBUG_LEDGER("[LEDGER][ADD_GUI_REQUEST]: buf: %s and buf length: %d\n",
    //     new_request->buf, buffer_size);

    return queue_req(new_request, caller_pid);

case_error:
    scheduler_wake_task(caller_pid);
    return STATUS_ERROR;
}

int ledger_ch_gui_color(uint32_t caller_pid, uint32_t type, uint32_t color) {
    if (caller_pid > MAX_TASKS) {
        goto case_error;
    }

    request_table *new_request = (request_table *)kmalloc(sizeof(request_table));
    if (new_request == NULL) {
        ERROR("[LEDGER][CH_GUI_COLOR]: could not allocate new request. Aborting\n");
        goto case_error;
    }

    memset(new_request, 0, sizeof(request_table));
    new_request->caller_pid   = caller_pid;
    new_request->clerk_pid    = gui_task_pid;
    new_request->request_type = type;
    new_request->color        = color;
    return queue_req(new_request, caller_pid);

case_error:
    scheduler_wake_task(caller_pid);
    return STATUS_ERROR;
}

int ledger_add_gui_free_req(uint32_t caller_pid, uint32_t target_pid) {
    if (caller_pid > MAX_TASKS) {
        return STATUS_ERROR;
    }

    request_table *new_request = (request_table *)kmalloc(sizeof(request_table));

    if (new_request == NULL) {
        ERROR("[LEDGER][ADD_GUI_FREE_REQ]: Could not allocate memory for the request\n");
        return STATUS_ERROR;
    }

    memset(new_request, 0, sizeof(request_table));
    new_request->caller_pid   = caller_pid;
    new_request->target_pid   = target_pid;
    new_request->request_type = FREE;
    new_request->clerk_pid    = gui_task_pid;

    return queue_req(new_request, caller_pid);
}
