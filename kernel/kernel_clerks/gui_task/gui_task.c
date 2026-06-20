#include "gui_task.h"
#include "blankie.h"
#include "config.h"
#include "fb.h"
#include "hail_mary.h"
#include "klog.h"
#include "ledger.h"

static uint32_t x_pos    = 0;
static uint32_t y_pos    = 0;
static uint32_t fg_color = 0;
static uint32_t bg_color = 0;

static int gui_draw_string(const char *str, uint32_t caller_pid) {
    __asm__ __volatile__("cli");

    // DEBUG_GUI_TASK("[GUI_TASK][DRAW_STRING]: trying to draw %s for %d\n", str, caller_pid);
    fb_draw_string(&x_pos, &y_pos, str, fg_color, bg_color);
    __asm__ __volatile__("sti");
    return STATUS_OK;
}

int gui_change_fg_color(uint32_t fg_color) {
}

int gui_change_bf_color(uint32_t bg_color) {
}

int gui_set_active_window(uint32_t wid) {
}

int gui_create_window(uint32_t owner_pid, uint32_t width, uint32_t height) {
}

static int gui_handle_request(request_table *req) {
    task_t *gui_task = task_get(gui_task_pid);

    // DEBUG_GUI_TASK("[GUI_TASK][HANDLE_REQUEST]: handling request %d\n", req->caller_pid);

    switch (req->request_type) {
    case WRITE:
        req->status        = (gui_draw_string(req->buf, req->caller_pid) == STATUS_OK) ? COMPLETE : TERMINATED;
        gui_task->priority = PRIORITY_NORMAL;
        scheduler_wake_task(req->caller_pid);
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
            DEBUG_GUI_TASK("[GUI_TASK][LOOP]: Request found!\n");
            if (req->status == PENDING || req->status == IN_PROGRESS) {
                gui_handle_request(req);
            }
        }

        blankie_activate(gui_task_pid);
    }
}

void gui_recovery() {
    DEBUG("[GUI_TASK][RECOVERY]:\n");
    blankie_activate(gui_task_pid);
}

void gui_init(task_t *gui_task) {
    blankie_register(gui_task_pid, gui_task->context.eip, gui_task->kernel_stack);
    register_hail_mary_function(gui_task_pid, gui_recovery);
    fg_color = fb_pack_color(255, 255, 255); // white text
    bg_color = fb_pack_color(0, 0, 0);       // black background
}