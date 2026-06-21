#include "gui_task.h"
#include "blankie.h"
#include "config.h"
#include "fb.h"
#include "hail_mary.h"
#include "klog.h"
#include "kmalloc.h"
#include "ledger.h"

static window_t *program_windows[MAX_TASKS];

static int gui_draw_string(const char *str, uint32_t caller_pid) {
    __asm__ __volatile__("cli");

    for (int i = 0; i < MAX_TASKS; i++) {
        if (program_windows[i] != NULL && program_windows[i]->owner_pid == caller_pid) {
            DEBUG_GUI_TASK("[GUI_TASK][DRAW_STRING]: Drawing for caller %d at table index %d\n", caller_pid, i);
            // DEBUG_GUI_TASK("[GUI_TASK][DRAW_STRING]: trying to draw %s for %d\n", str, caller_pid);
            window_t *entry = program_windows[i];
            uint32_t x_pos  = entry->x_offset;
            uint32_t y_pos  = entry->y_offset;

            fb_draw_string(&x_pos, &y_pos, entry->width, entry->height, str, entry->fg_color, entry->bg_color);

            DEBUG_GUI_TASK("[GUI_TASK][DRAW_STRING]: New x position %d\n", x_pos);
            DEBUG_GUI_TASK("[GUI_TASK][DRAW_STRING]: New y position %d\n", y_pos);

            entry->x_offset = x_pos;
            entry->y_offset = y_pos;
            break;
        }
    }

    __asm__ __volatile__("sti");
    return STATUS_OK;
}

static int gui_change_fg_color(uint32_t owner_pid, int32_t fg_color) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (program_windows[i] != NULL && program_windows[i]->owner_pid == owner_pid) {
            program_windows[i]->fg_color = fg_color;
            return STATUS_OK;
        }
    }
    return STATUS_ERROR;
}

static int gui_change_bg_color(uint32_t owner_pid, uint32_t bg_color) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (program_windows[i] != NULL && program_windows[i]->owner_pid == owner_pid) {
            program_windows[i]->bg_color = bg_color;
            return STATUS_OK;
        }
    }
    return STATUS_ERROR;
}

static int gui_set_active_window(uint32_t wid) {
}

static int gui_create_window(uint32_t owner_pid, uint32_t width, uint32_t height) {
    DEBUG_GUI_TASK("[GUI_TASK][CREATE_WINDOW]: Trying to initialize a window\n");
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (program_windows[i] == NULL) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        ERROR("[GUI_TASK][CREATE_WINDOW]: There was no space on the window table\n");
        return STATUS_ERROR;
    }

    window_t *entry = (window_t *)kmalloc(sizeof(window_t));

    if (entry == NULL) {
        ERROR("[GUI_TASK][CREATE_WINDOW]: Reserving memory for the window table failed. Aborting\n");
        return STATUS_ERROR;
    }

    DEBUG_GUI_TASK("[GUI_TASK][CREATE_WINDOW]: Slot and memory allocated, setting values\n");
    entry->wid       = slot;
    entry->owner_pid = owner_pid;
    entry->width     = width;
    entry->height    = height;
    entry->x_offset  = 0;
    entry->y_offset  = 0;
    entry->z_index   = 1;
    entry->fg_color  = fb_pack_color(255, 255, 255);
    entry->bg_color  = fb_pack_color(23, 29, 184);

    fb_fill_rect(entry->x_offset, entry->y_offset, entry->width, entry->height, entry->bg_color);

    DEBUG_GUI_TASK("[GUI_TASK][CREATE_WINDOW]: Window created to table index: %d\n", slot);
    program_windows[slot] = entry;

    return STATUS_OK;
}

static int gui_delete_window(uint32_t owner_pid) {
    DEBUG_GUI_TASK("[GUI_TASK][DELETE_WINDOW]: Deleting %d window\n", owner_pid);

    for (int i = 0; i < MAX_TASKS; i++) {
        if (program_windows[i] != NULL && program_windows[i]->owner_pid == owner_pid) {
            DEBUG_GUI_TASK("[GUI_TASK][DELETE_WINDOW]: Target found at: %d\n", i);
            kfree(program_windows[i]);
            program_windows[i] = NULL;
            DEBUG_GUI_TASK("[GUI_TASK][DELETE_WINDOW]: deletion succesfull\n");
            return STATUS_OK;
        }
    }

    DEBUG_GUI_TASK("[GUI_TASK][DELETE_WINDOW]: No window found with given %d\n", owner_pid);
    return STATUS_ERROR;
}

static int gui_handle_request(request_table *req) {
    task_t *gui_task = task_get(gui_task_pid);

    DEBUG_GUI_TASK("[GUI_TASK][HANDLE_REQUEST]: handling request %d with type : %d\n", req->caller_pid, req->request_type);

    switch (req->request_type) {
    case WRITE:
        req->status        = (gui_draw_string(req->buf, req->caller_pid) == STATUS_OK) ? COMPLETE : TERMINATED;
        gui_task->priority = PRIORITY_NORMAL;
        scheduler_wake_task(req->caller_pid);
        return STATUS_OK;

    case CREATE:
        req->status        = (gui_create_window(req->caller_pid, 800, 600) == STATUS_OK) ? COMPLETE : FAILED;
        gui_task->priority = PRIORITY_NORMAL;
        return STATUS_OK;
    case DELETE:
        req->status        = (gui_delete_window(req->caller_pid) == STATUS_OK) ? TERMINATED : FAILED;
        gui_task->priority = PRIORITY_NORMAL;
        return STATUS_OK;

    default:

        req->status = TERMINATED;
        scheduler_wake_task(req->caller_pid);
        return STATUS_ERROR;
    }
}

void gui_task_loop() {
    while (1) {
        request_table *req = ledger_fetch_next_req(gui_task_pid);
        if (req != NULL) {
            if (req->status == PENDING || req->status == IN_PROGRESS) {
                gui_handle_request(req);
            }
        }

        blankie_activate(gui_task_pid);
    }
}

void gui_recovery() {
    DEBUG_GUI_TASK("[GUI_TASK][RECOVERY]:\n");
    blankie_activate(gui_task_pid);
}

void gui_init(task_t *gui_task) {
    DEBUG_GUI_TASK("[GUI_TASK][INIT]: Initializing GUI\n");
    blankie_register(gui_task_pid, gui_task->context.eip, gui_task->kernel_stack);
    register_hail_mary_function(gui_task_pid, gui_recovery);
    uint32_t fg_color = fb_pack_color(115, 145, 125); // white text
    uint32_t bg_color = fb_pack_color(70, 50, 100);   // black background

    DEBUG_GUI_TASK("[GUI_TASK][INIT]: Creating main screen\n");
    gui_create_window(gui_task_pid, fb.width, fb.height);
    gui_change_fg_color(gui_task_pid, fg_color);
    gui_change_bg_color(gui_task_pid, bg_color);

    window_t *entry = program_windows[0];

    fb_fill_rect(entry->x_offset, entry->y_offset, entry->width, entry->height, fg_color);

    // fb_clear(fg_color);

    DEBUG_GUI_TASK("[GUI_TASK][INIT]: Screen succesfully initialized\n");
}