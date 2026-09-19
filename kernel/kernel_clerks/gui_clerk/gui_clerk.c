#include "gui_clerk.h"
#include "blankie.h"
#include "config.h"
#include "fb.h"
#include "hail_mary.h"
#include "klog.h"
#include "kmalloc.h"
#include "kstring.h"
#include "ledger.h"
#include "sched.h"
#include "shared.h"
#include "taavi.h"
#include <stdint.h>

/**
 * Gui_task
 * Design & Implementation:
 * @author: A.H, 2026
 */

static blueprint_t *program_windows[MAX_TASKS];
static uint32_t bg_color       = 0;
static int hail_mary_act_count = 0;

static inline void clamp_dimensions(uint16_t *width, uint16_t *height, uint32_t max_w, uint32_t max_h) {
    if (*width > max_w)
        *width = max_w;
    if (*height > max_h)
        *height = max_h;
}

static void clamp_bounds(uint16_t *x, uint16_t *y, uint16_t *w, uint16_t *h, uint32_t max_w, uint32_t max_h) {
    clamp_dimensions(w, h, max_w, max_h);

    if (*x + *w > max_w) {
        *x = max_w - *w;
    }
    if (*y + *h > max_h) {
        *y = max_h - *h;
    }
}

static void clip_rect(uint16_t *x, uint16_t *y, uint16_t *w, uint16_t *h, uint32_t max_w, uint32_t max_h) {
    if (*x >= max_w || *y >= max_h) {
        *w = 0;
        *h = 0;
        return;
    }
    if (*x + *w > max_w)
        *w = max_w - *x;
    if (*y + *h > max_h)
        *h = max_h - *y;
}

static int copy_pixels_to_screen(blueprint_t *entry) {
    if (entry == NULL) {
        return STATUS_ERROR;
    }

    for (uint32_t row = 0; row < entry->height; row++) {
        memcpy(
            (uint32_t *)fb.virt_addr + (entry->screen_y + row) * (fb.pitch / 4) + entry->screen_x, // dst
            entry->pixels + row * entry->width,                                                    // src
            entry->width * 4);                                                                     // len
    }

    return STATUS_OK;
}

/**
 * gui_draw_string - used to put chars to screen.
 * @param *req: holds request information
 *
 * Description:
 * Function searches from the program windows the entry that owner corresponds to caller pid.
 * After that it checks if the callers pixel buffer is made. If not the function returns early without drawing
 * If it is allocated the function calls on fb_draw_string to but the string at the correct x and y position in pixels buffer.
 * Then it copies the pixels buffer to frame buffer virtual address.
 *
 * Return: If successful return STATUS_OK || if unsuccessful return STATUS_ERROR.
 */
static int gui_draw_string(const request_table *req) {
    // DEBUG_GUI_TASK("[GUI_TASK][DRAW_STRING]: Drawing for caller %d\n", caller_pid);
    //  DEBUG_GUI_TASK("[GUI_TASK][DRAW_STRING]: trying to draw %s\n", req->buf);

    if (req == NULL) {
        return STATUS_ERROR;
    }

    blueprint_t *entry = program_windows[req->struct_key];

    if (entry == NULL || entry->pixels == NULL) {
        DEBUG_GUI_TASK("[GUI_TASK][DRAW_STRING]: Pixel buffer was null, cant draw\n");
        return STATUS_ERROR;
    }

    if (entry->owner_pid != req->caller_pid) {
        ERROR("[GUI_TASK] Caller tried to access somebody elses window.\n");
        return STATUS_ERROR;
    }

    if (req->buf == NULL || req->x >= entry->width || req->y >= entry->height) {
        ERROR("[GUI_TASK][DRAW_STRING]: request was invalid\n");
        return STATUS_ERROR;
    }

    fb_draw_string(entry->pixels, req->x, req->y, entry->width, req->buf,
                   req->fg_color, req->bg_color);

    return copy_pixels_to_screen(entry);
}

/**
 * gui_delete_window - Used when task is killed, regardless if it has a window or not.
 * @param owner_pid: window entry owner pid
 *
 * Description:
 * Function searches from the program_windows array the correct entry and frees the allocated memory
 * aswell as clears the drawn pixels from the screen where the app used to be.
 *
 * Return: If successful return STATUS_OK || if unsuccessful return STATUS_ERROR.
 */
static int gui_delete_window(uint32_t target_pid) {
    DEBUG_GUI_TASK("[GUI_TASK][DELETE_WINDOW]: Deleting %d window\n", target_pid);

    for (int i = 0; i < MAX_TASKS; i++) {
        if (program_windows[i] != NULL && program_windows[i]->owner_pid == target_pid) {
            DEBUG_GUI_TASK("[GUI_TASK][DELETE_WINDOW]: Target found at: %d\n", i);
            blueprint_t *entry = program_windows[i];

            if (fb_clear(entry->pixels, entry->width, entry->height, bg_color) == STATUS_ERROR) {
                ERROR("[GUI_TASK][DELETE_WINDOW]: Could not clear window, aborting\n");
                return STATUS_ERROR;
            }

            if (copy_pixels_to_screen(entry) == STATUS_ERROR) {
                ERROR("[GUI_TASK][DELETE_WINDOW]: Failed the copy pixels to screen\n");
                return STATUS_ERROR;
            }

            if (entry->pixels != NULL) {
                DEBUG_GUI_TASK("[GUI_TASK][DELETE_WINDOW]: Freeing reserved pixels\n");
                kfree(entry->pixels);
            }
            kfree(entry);
            program_windows[i] = NULL;

            DEBUG_GUI_TASK("[GUI_TASK][DELETE_WINDOW]: deletion succesfull\n");
            return STATUS_OK;
        }
    }

    DEBUG_GUI_TASK("[GUI_TASK][DELETE_WINDOW]: No window found with given %d\n", target_pid);
    return STATUS_ERROR;
}

/**
 * gui_create_window_entry - used if a task wishes to create a window to screen.
 * @param req: hold metadata of the made reguest.
 *
 * Description:
 * Function searches for an empty slot from the program_windows. If found it allocates new entry
 * then it floods the entry info from the given parameters as well as allocate the pixel buffer
 * that is calculated by the needed size of width * height * 4
 * After entry is complete it uses entry program_window index as direct index to compositor array.
 * Add the entry as pointer to compositor array and the x and y coordinates.
 *
 * Return: If successful return STATUS_OK || if unsuccessful return STATUS_ERROR.
 */
static int gui_create_window_entry(request_table *req) {

    DEBUG_GUI_TASK("[GUI_TASK][CREATE_WINDOW]: Trying to initialize a window\n");
    if (req == NULL) {
        return STATUS_ERROR;
    }

    int slot = -1;

    for (int i = 0; i < MAX_TASKS; i++) {
        if (program_windows[i] == NULL) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        ERROR("[GUI_TASK][CREATE_WINDOW]: There was no space on the window table\n");
        req->struct_key = STATUS_ERROR;
        return STATUS_ERROR;
    }

    blueprint_t *entry = (blueprint_t *)kmalloc(sizeof(blueprint_t));

    if (entry == NULL) {
        ERROR("[GUI_TASK][CREATE_WINDOW]: Reserving memory for the window table failed. Aborting\n");
        req->struct_key = STATUS_ERROR;
        return STATUS_ERROR;
    }

    DEBUG_GUI_TASK("[GUI_TASK][CREATE_WINDOW]: Slot and memory allocated, setting values\n");
    entry->owner_pid = req->caller_pid;

    clamp_bounds(&req->x, &req->y, &req->width, &req->height, fb.width, fb.height);
    entry->width         = req->width;
    entry->height        = req->height;
    entry->screen_x      = req->x;
    entry->screen_y      = req->y;

    uint32_t pixels_size = entry->width * entry->height * 4;
    entry->pixels        = (uint32_t *)kmalloc(pixels_size);

    if (entry->pixels == NULL) {
        DEBUG_GUI_TASK("[GUI_TASK][PAINT_WINDOW]: Unable to allocate memory for pixels\n");
        req->struct_key = STATUS_ERROR;
        kfree(entry);
        return STATUS_ERROR;
    }
    memset(entry->pixels, 0, pixels_size);
    program_windows[slot] = entry;

    DEBUG_GUI_TASK("[GUI_TASK][CREATE_WINDOW]: Window created to table index: %d\n", slot);
    req->struct_key = slot;
    return STATUS_OK;
}

static int gui_scroll_window(request_table *req) {

    if (req == NULL) {
        return STATUS_ERROR;
    }

    blueprint_t *entry = program_windows[req->struct_key];

    if (entry == NULL) {
        return STATUS_ERROR;
    }

    if (entry->owner_pid != req->caller_pid) {
        ERROR("[GUI_TASK] Caller tried to access somebody elses window.\n");
        return STATUS_ERROR;
    }

    clamp_dimensions(&req->width, &req->height, fb.width, fb.height);

    return fb_scroll_down(entry->pixels, req->x, req->y, req->width, req->height,
                          entry->width, req->bg_color);
}

static int gui_paint_rectangle(request_table *req) {

    blueprint_t *entry = program_windows[req->struct_key];

    if (entry == NULL || entry->pixels == NULL) {
        return STATUS_ERROR;
    }

    if (entry->owner_pid != req->caller_pid) {
        ERROR("[GUI_TASK] Caller tried to access somebody elses window.\n");
        return STATUS_ERROR;
    }

    clip_rect(&req->x, &req->y, &req->width, &req->height, fb.width, fb.height);

    fb_fill_rect(entry->pixels, req->x, req->y, req->width,
                 req->height, entry->width, entry->height, req->bg_color);

    //  DEBUG_GUI_TASK("[GUI_TASK][PAINT_RECT]: Window painted successfully to screen!\n");
    return copy_pixels_to_screen(entry);
}

static int gui_draw_sprite(request_table *req) {

    blueprint_t *entry = program_windows[req->struct_key];

    if (entry == NULL || entry->pixels == NULL || req->pixels == NULL) {
        return STATUS_ERROR;
    }

    if (entry->owner_pid != req->caller_pid) {
        ERROR("[GUI_TASK] Caller tried to access somebody elses window.\n");
        return STATUS_ERROR;
    }

    for (uint32_t row = 0; row < req->height; row++) {
        for (uint32_t col = 0; col < req->width; col++) {
            uint32_t color = req->pixels[row * req->width + col];
            if (color != TRANSPARENT) {
                fb_fill_rect((uint32_t *)entry->pixels,
                             req->x + col * req->scale, req->y + row * req->scale,
                             req->scale, req->scale,
                             entry->width, entry->height,
                             color);
            }
        }
    }

    return copy_pixels_to_screen(entry);
}

static int gui_resize_window(request_table *req) {

    blueprint_t *entry = program_windows[req->struct_key];

    if (entry == NULL || entry->pixels == NULL) {
        return STATUS_ERROR;
    }

    if (entry->owner_pid != req->caller_pid) {
        ERROR("[GUI_TASK] Caller tried to access somebody elses window.\n");
        return STATUS_ERROR;
    }

    clamp_dimensions(&req->width, &req->height, fb.width, fb.height);

    uint32_t pixels_size       = req->width * req->height * 4;

    uint32_t *new_pixel_buffer = (uint32_t *)kmalloc(pixels_size);
    if (new_pixel_buffer == NULL) {
        ERROR("[GUI_TASK][RESIZE]: Could not allocate new pixel buffer\n");
        return STATUS_ERROR;
    }
    memset(new_pixel_buffer, 0, pixels_size);

    if (fb_fill_rect((uint32_t *)fb.virt_addr, entry->screen_x, entry->screen_y, entry->width,
                     entry->height, fb.width, fb.height, bg_color) == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    uint32_t copy_width  = (entry->width < req->width) ? entry->width : req->width;
    uint32_t copy_height = (entry->height < req->height) ? entry->height : req->height;

    for (uint32_t row = 0; row < copy_height; row++) {
        memcpy(
            new_pixel_buffer + row * req->width, // dst
            entry->pixels + row * entry->width,  // src
            copy_width * 4);                     // len
    }

    kfree(entry->pixels);
    entry->pixels = new_pixel_buffer;

    entry->width  = req->width;
    entry->height = req->height;

    clamp_bounds(&entry->screen_x, &entry->screen_y, &req->width,
                 &req->height, fb.width, fb.height);

    //    DEBUG_GUI_TASK("[GUI_TASK][RESIZE]: successfully resized window\n");
    return copy_pixels_to_screen(entry);
}

static int gui_move_task_window(const request_table *req) {
    blueprint_t *entry = program_windows[req->struct_key];

    if (entry == NULL || entry->pixels == NULL) {
        return STATUS_ERROR;
    }

    if (entry->owner_pid != req->caller_pid) {
        ERROR("[GUI_TASK] Caller tried to access somebody elses window.\n");
        return STATUS_ERROR;
    }

    if (fb_fill_rect((uint32_t *)fb.virt_addr, entry->screen_x, entry->screen_y, entry->width,
                     entry->height, fb.width, fb.height, bg_color) == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    entry->screen_x = req->x;
    entry->screen_y = req->y;

    clamp_bounds(&entry->screen_x, &entry->screen_y, &entry->width, &entry->height, fb.width, fb.height);

    //  DEBUG_GUI_TASK("[GUI_TASK][MOVE]: successfully moved window\n");
    return copy_pixels_to_screen(entry);
}

static void gui_handle_request(request_table *req) {
    task_t *gui_task = task_get(gui_task_pid);

    if (gui_task == NULL || req == NULL)
        return;

    switch (req->request_type) {
    case WRITE_AT:
        req->status = (gui_draw_string(req) == STATUS_OK) ? COMPLETE : FAILED;
        goto after_req_steps;
    case CREATE:
        req->status = (gui_create_window_entry(req) == STATUS_OK) ? COMPLETE : FAILED;
        goto after_req_steps;
    case FREE:
        req->status = (gui_delete_window(req->target_pid) == STATUS_OK) ? COMPLETE : FAILED;
        goto after_req_steps;
    case PAINT_WINDOW:
        req->status = (gui_paint_rectangle(req) == STATUS_OK) ? COMPLETE : FAILED;
        goto after_req_steps;
    case DRAW:
        req->status = (gui_draw_sprite(req) == STATUS_OK) ? COMPLETE : FAILED;
        goto after_req_steps;
    case SCROLL_DOWN:
        req->status = (gui_scroll_window(req) == STATUS_OK) ? COMPLETE : FAILED;
        goto after_req_steps;
    case MOVE:
        req->status = (gui_move_task_window(req) == STATUS_OK) ? COMPLETE : FAILED;
        goto after_req_steps;
    case RESIZE:
        req->status = (gui_resize_window(req) == STATUS_OK) ? COMPLETE : FAILED;
        goto after_req_steps;

    default:
        req->status = FAILED;
        goto after_req_steps;
    }
after_req_steps:
    gui_task->priority = PRIORITY_NORMAL;
    scheduler_wake_task(req->caller_pid);
    return;
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

static void gui_recovery() {
    ERROR("\n[GUI_TASK][RECOVERY]: activated %d times\n", hail_mary_act_count);

    hail_mary_act_count++;
    ledger_check_request(gui_task_pid);
    blankie_activate(gui_task_pid);
}

void gui_init(const task_t *gui_task) {
    DEBUG_GUI_TASK("[GUI_TASK][INIT]: Initializing GUI\n");
    blankie_register(gui_task_pid, gui_task->context.eip, gui_task->kernel_stack);
    register_hail_mary_function(gui_task_pid, gui_recovery);
    bg_color = COLOR_DARKER_GREEN;

    DEBUG_GUI_TASK("[GUI_TASK][INIT]: Initializing program windows\n");
    memset(program_windows, 0, sizeof(program_windows));

    DEBUG_GUI_TASK("[GUI_TASK][INIT]: Creating main screen\n");
    fb_clear((uint32_t *)fb.virt_addr, fb.width, fb.height, bg_color);

    int y = 5;
    int x = fb.width - 133;

    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 32; col++) {
            if (taavi[row][col] != TRANSPARENT) {
                fb_fill_rect((uint32_t *)fb.virt_addr,
                             x + col * 4, y + row * 4,
                             4, 4,
                             fb.width, fb.height,
                             taavi[row][col]);
            }
        }
    }

    DEBUG_GUI_TASK("[GUI_TASK][INIT]: Screen succesfully initialized\n");
}
