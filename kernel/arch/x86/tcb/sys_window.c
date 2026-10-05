#include "config.h"
#include "fb.h"
#include "klog.h"
#include "kstring.h"
#include "ledger.h"
#include "sched.h"
#include "shared.h"
#include "syscall.h"
#include "task.h"
#include <stdint.h>

/**
 * gui_draw_string - used to put chars to screen.
 * @param *params: holds request information
 *
 * Description:
 * Function searches from the program windows the entry that owner corresponds to caller pid.
 * After that it checks if the callers pixel buffer is made. If not the function returns early without drawing
 * If it is allocated the function calls on fb_draw_string to but the string at the correct x and y position in pixels buffer.
 * Then it copies the pixels buffer to frame buffer virtual address.
 *
 * Return: If successful return STATUS_OK || if unsuccessful return STATUS_ERROR.
 */
static int draw_string(const gui_params_pack *params, uint32_t caller_pid) {
    // DEBUG_SYSCALL("[SYS_WI][DRAW_STRING]: Drawing for caller %d\n", caller_pid);
    //  DEBUG_SYSCALL("[SYS_WI][DRAW_STRING]: trying to draw %s\n", req->buf);

    if (params == NULL) {
        return STATUS_ERROR;
    }

    blueprint_t *entry = program_windows[params->struct_key];

    if (entry == NULL || entry->pixels == NULL) {
        DEBUG_SYSCALL("[SYS_WI][DRAW_STRING]: Pixel buffer was null, cant draw\n");
        return STATUS_ERROR;
    }

    if (entry->owner_pid != caller_pid) {
        ERROR("[SYS_WI] Caller tried to access somebody elses window.\n");
        return STATUS_ERROR;
    }

    if (params->buf == NULL || params->x >= entry->width || params->y >= entry->height) {
        ERROR("[SYS_WI][DRAW_STRING]: request was invalid\n");
        return STATUS_ERROR;
    }

    fb_draw_string(entry->pixels, params->x, params->y, entry->width, params->buf,
                   params->fg_color, params->bg_color);

    entry->dirty_flag = 1;
    return STATUS_OK;
}

static int scroll_window(const gui_params_pack *params, uint32_t caller_pid) {

    if (params == NULL) {
        return STATUS_ERROR;
    }

    blueprint_t *entry = program_windows[params->struct_key];

    if (entry == NULL) {
        return STATUS_ERROR;
    }

    if (entry->owner_pid != caller_pid) {
        ERROR("[SYS_WI] Caller tried to access somebody elses window.\n");
        return STATUS_ERROR;
    }

    if (params->width > entry->width || params->height > entry->height ||
        params->x >= entry->width || params->y >= entry->height || params->x + params->width > entry->width ||
        params->y + params->height > entry->height || params->width == 0 || params->height == 0) {
        DEBUG_SYSCALL("[SYS_WI][SCROLL_WINDOW]: Invalid window dimensions\n");
        return STATUS_ERROR;
    }

    fb_scroll_down(entry->pixels, params->x, params->y, params->width, params->height,
                   entry->width, params->bg_color);

    entry->dirty_flag = 1;
    return STATUS_OK;
}

static int paint_rectangle(const gui_params_pack *params, uint32_t caller_pid) {

    blueprint_t *entry = program_windows[params->struct_key];

    if (entry == NULL || entry->pixels == NULL) {
        return STATUS_ERROR;
    }

    if (entry->owner_pid != caller_pid) {
        ERROR("[SYS_WI] Caller tried to access somebody elses window.\n");
        return STATUS_ERROR;
    }

    if (params->width > entry->width || params->height > entry->height ||
        params->x >= entry->width || params->y >= entry->height || params->x + params->width > entry->width ||
        params->y + params->height > entry->height || params->width == 0 || params->height == 0) {
        DEBUG_SYSCALL("[SYS_WI][PAINT_RECT]: Invalid window dimensions\n");
        return STATUS_ERROR;
    }

    fb_fill_rect(entry->pixels, params->x, params->y, params->width,
                 params->height, entry->width, entry->height, params->bg_color);

    //  DEBUG_SYSCALL("[SYS_WI][PAINT_RECT]: Window painted successfully to screen!\n");
    entry->dirty_flag = 1;
    return STATUS_OK;
}

static int draw_sprite(const gui_params_pack *params, uint32_t caller_pid) {

    blueprint_t *entry = program_windows[params->struct_key];

    if (entry == NULL || entry->pixels == NULL || params->pixels == NULL) {
        return STATUS_ERROR;
    }

    if (entry->owner_pid != caller_pid) {
        ERROR("[SYS_WI] Caller tried to access somebody elses window.\n");
        return STATUS_ERROR;
    }

    for (uint32_t row = 0; row < params->height; row++) {
        for (uint32_t col = 0; col < params->width; col++) {
            uint32_t color = params->pixels[row * params->width + col];
            if (color != TRANSPARENT) {
                fb_fill_rect((uint32_t *)entry->pixels,
                             params->x + col * params->scale, params->y + row * params->scale,
                             params->scale, params->scale,
                             entry->width, entry->height,
                             color);
            }
        }
    }

    entry->dirty_flag = 1;
    return STATUS_OK;
}

static int move_task_window(const gui_params_pack *params, uint32_t caller_pid) {
    blueprint_t *entry = program_windows[params->struct_key];

    if (entry == NULL || entry->pixels == NULL) {
        return STATUS_ERROR;
    }

    if (entry->owner_pid != caller_pid) {
        ERROR("[SYS_WI] Caller tried to access somebody elses window.\n");
        return STATUS_ERROR;
    }

    if (params->x >= fb.width || params->y >= fb.height || (entry->width + params->x) > fb.width ||
        (entry->height + params->y) > fb.height) {
        DEBUG_SYSCALL("[SYS_WI][MOVE]: Invalid window dimensions\n");
        return STATUS_ERROR;
    }

    if (fb_fill_rect((uint32_t *)fb.virt_addr, entry->screen_x, entry->screen_y, entry->width,
                     entry->height, fb.width, fb.height, COLOR_DARKER_GREEN) == STATUS_ERROR) {
        return STATUS_ERROR;
    }
    entry->screen_x   = params->x;
    entry->screen_y   = params->y;

    //  DEBUG_SYSCALL("[SYS_WI][MOVE]: successfully moved window\n");
    entry->dirty_flag = 1;
    return STATUS_OK;
}

/**
 * sys_configure_window - When userspace task wants to make changes to their window.
 *
 * Description:
 * This function takes in pack of params. They contain opcode and values (for example width/height
 * x/y) the caller is giving to gui. Gui then complites request and caller collects the results
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
int32_t sys_window(struct registers *r) {
    // DEBUG_SYSCALL("[SYSCALL][CONWI]\n");
    gui_params_pack *params = (gui_params_pack *)r->ebx;
    task_t *current         = scheduler_get_current_task();

    if (current == NULL || params == NULL) {
        ERROR("[SYSCALL][SYS_CONWI]: invalid input params. Aborting\n");
        return STATUS_ERROR;
    }

    switch (params->opcode) {
    case WRITE_AT:
        return draw_string(params, current->pid);
    case PAINT_WINDOW:
        return paint_rectangle(params, current->pid);
    case DRAW:
        return draw_sprite(params, current->pid);
    case SCROLL_DOWN:
        return scroll_window(params, current->pid);
    case MOVE:
        return move_task_window(params, current->pid);
    }

    if (ledger_add_gui_req(current->pid, params) == STATUS_ERROR) {
        return STATUS_ERROR;
    }
    scheduler_set_task_state(TASK_BLOCKED);
    scheduler_yield(r);
    return ledger_collect(current->pid, gui_task_pid, params->buf);
}
