#include "gui_task.h"
#include "blankie.h"
#include "config.h"
#include "fb.h"
#include "hail_mary.h"
#include "klog.h"
#include "kmalloc.h"
#include "kstring.h"
#include "ledger.h"

/**
 * Gui_task
 * Design & Implementation:
 * @author: A.H, 2026
 */

static window_t *program_windows[MAX_TASKS];
static blueprint_t compositor[MAX_TASKS];
static uint32_t bg_color       = 0;
static uint32_t fg_color       = 0;
static int hail_mary_act_count = 0;

static int copy_pixels_to_screen(window_t *entry) {
    if (entry == NULL) {
        return STATUS_ERROR;
    }

    for (uint32_t row = 0; row < entry->height; row++) {
        memcpy(
            (uint32_t *)fb.virt_addr + (compositor[entry->wid].screen_y + row) * (fb.pitch / 4) + compositor[entry->wid].screen_x, // dst
            entry->pixels + row * entry->width,                                                                                    // src
            entry->width * 4);                                                                                                     // len
    }

    return STATUS_OK;
}

/**
 * gui_draw_string - used to put chars to screen.
 * @param str: holds the string.
 * @param caller_pid: used to know whos screen to edit.
 *
 * Description:
 * Function searches from the program windows the entry thats owner corresponds to caller pid.
 * After that it checks if the callers pixel buffer is made. If not the function returns early witout drawing
 * If it is allocated the function calls on fb_draw_string to but the string at the correct x and y position in pixels buffer.
 * Then it copies the pixels buffer to framebuffer virtual address.
 *
 * Return: If successfull return STATUS_OK || if unsuffessfull return STATUS_ERROR.
 */
static int gui_draw_string(const char *str, uint32_t caller_pid) {
    // DEBUG_GUI_TASK("[GUI_TASK][DRAW_STRING]: Drawing for caller %d\n", caller_pid);
    for (int i = 0; i < MAX_TASKS; i++) {
        if (program_windows[i] != NULL && program_windows[i]->owner_pid == caller_pid) {
            // DEBUG_GUI_TASK("[GUI_TASK][DRAW_STRING]: trying to draw %s for %d\n", str, caller_pid);
            window_t *entry = program_windows[i];

            if (entry->pixels == NULL) {
                DEBUG_GUI_TASK("[GUI_TASK][DRAW_STRING]: Pixel buffer was null, cant draw\n");
                return STATUS_ERROR;
            }

            uint32_t x_pos = entry->x_offset;
            uint32_t y_pos = entry->y_offset;

            fb_draw_string(entry->pixels, &x_pos, &y_pos, entry->width, entry->height, str, entry->fg_color, entry->bg_color);

            // DEBUG_GUI_TASK("[GUI_TASK][DRAW_STRING]: New x position %d\n", x_pos);
            // DEBUG_GUI_TASK("[GUI_TASK][DRAW_STRING]: New y position %d\n", y_pos);

            entry->x_offset = x_pos;
            entry->y_offset = y_pos;

            if (copy_pixels_to_screen(entry) == STATUS_ERROR) {
                return STATUS_ERROR;
            }

            break;
        }
    }
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

/**
 * gui_delete_window - Used when task is killed, regardles if it has a window or not.
 * @param owner_pid: window entry owner pid
 *
 * Description:
 * Function searches from the program_windows array the correct entry and frees the allocated memory
 * aswell as clears the drawn pixels from the screen where the app used to be.
 *
 * Return: If successfull return STATUS_OK || if unsuffessfull return STATUS_ERROR.
 */
static int gui_delete_window(uint32_t owner_pid) {
    DEBUG_GUI_TASK("[GUI_TASK][DELETE_WINDOW]: Deleting %d window\n", owner_pid);

    for (int i = 0; i < MAX_TASKS; i++) {
        if (program_windows[i] != NULL && program_windows[i]->owner_pid == owner_pid) {
            DEBUG_GUI_TASK("[GUI_TASK][DELETE_WINDOW]: Target found at: %d\n", i);
            window_t *entry = program_windows[i];

            if (fb_clear(entry->pixels, entry->width, entry->height, fg_color) == STATUS_ERROR) {
                ERROR("[GUI_TASK][DELETE_WINDOW]: Could not clear window, aborting\n");
                return STATUS_ERROR;
            }

            if (copy_pixels_to_screen(entry) == STATUS_ERROR) {
                ERROR("[GUI_TASK][DELETE_WINDOW]: Failed the copy pixels to screen\n");
                return STATUS_ERROR;
            }

            compositor[entry->wid].entry    = NULL;
            compositor[entry->wid].screen_x = 0;
            compositor[entry->wid].screen_y = 0;

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

    DEBUG_GUI_TASK("[GUI_TASK][DELETE_WINDOW]: No window found with given %d\n", owner_pid);
    return STATUS_OK;
}

/**
 * gui_create_window_entry - used if a task wishes to create a window to screen.
 * @param owner_pid: window entry owner pid
 * @param width: window width
 * @param height: window height
 * @param x: compositor coordinate x
 * @param y: compositor coordinate y
 *
 * Description:
 * Function searches for an empty slot from the program_windows. If found it allocates new entry
 * then it floods the entrys info from the given parameters as well as allocate the pixel buffer
 * that is calculated by the needed size of width * height * 4
 * After entry is complete it uses entrys program_window index as direct index to compositor array.
 * Add the entry as pointer to compositor array and the x and y coordinates.
 *
 * Return: If successfull return STATUS_OK || if unsuffessfull return STATUS_ERROR.
 */
static int gui_create_window_entry(uint32_t owner_pid, uint32_t width, uint32_t height, uint32_t x, uint32_t y) {
    DEBUG_GUI_TASK("[GUI_TASK][CREATE_WINDOW]: Trying to initialize a window\n");

    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (program_windows[i] != NULL && program_windows[i]->owner_pid == owner_pid) {
            gui_delete_window(owner_pid);
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        for (int i = 0; i < MAX_TASKS; i++) {
            if (program_windows[i] == NULL) {
                slot = i;
                break;
            }
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
    entry->wid           = slot;
    entry->owner_pid     = owner_pid;
    entry->width         = width;
    entry->height        = height;
    entry->x_offset      = DEFAULT_HORIZONTAL_PADDING;
    entry->y_offset      = DEFAULT_VERTICAL_PADDING;
    entry->z_index       = 1;
    entry->fg_color      = fb_pack_color(255, 255, 255);
    entry->bg_color      = fb_pack_color(0, 0, 0);

    uint32_t pixels_size = entry->width * entry->height * 4;
    entry->pixels        = (uint32_t *)kmalloc(pixels_size);

    if (entry->pixels == NULL) {
        DEBUG_GUI_TASK("[GUI_TASK][PAINT_WINDOW]: Unable to allocate memory for pixels\n");
        return STATUS_ERROR;
    }
    memset(entry->pixels, 0, pixels_size);

    DEBUG_GUI_TASK("[GUI_TASK][CREATE_WINDOW]: Window created to table index: %d\n", slot);
    DEBUG_GUI_TASK("[GUI_TASK][CREATE_WINDOW]: Filling compositor info\n");
    program_windows[slot]     = entry;
    compositor[slot].entry    = program_windows[slot];
    compositor[slot].screen_x = x;
    compositor[slot].screen_y = y;
    DEBUG_GUI_TASK("[GUI_TASK][CREATE_WINDOW]: Window created\n");
    return STATUS_OK;
}

static int gui_paint_window_to_screen(request_table *req) {

    DEBUG_GUI_TASK("[GUI_TASK][PAINT_WINDOW]: Painting a window for %d to screen!\n", req->caller_pid);
    for (int i = 0; i < MAX_TASKS; i++) {
        if (program_windows[i] != NULL && program_windows[i]->owner_pid == req->caller_pid) {
            window_t *entry = program_windows[i];

            if (entry->pixels == NULL) {
                return STATUS_ERROR;
            }

            fb_fill_rect(entry->pixels, req->x, req->y, req->width, req->height, entry->width, entry->height, entry->bg_color);

            if (copy_pixels_to_screen(entry) == STATUS_ERROR) {
                return STATUS_ERROR;
            }

            DEBUG_GUI_TASK("[GUI_TASK][PAINT_WINDOW]: Window painted successfully to screen!\n");
            return STATUS_OK;
        }
    }

    return STATUS_OK;
}

static int gui_draw_sprite(request_table *req) {

    for (int i = 0; i < MAX_TASKS; i++) {
        if (program_windows[i] != NULL && program_windows[i]->owner_pid == req->caller_pid) {
            window_t *entry = program_windows[i];
            for (int row = 0; row < 32; row++) {
                for (int col = 0; col < 32; col++) {
                    fb_fill_rect((uint32_t *)entry->pixels,
                                 req->x + col * req->scale, req->y + row * req->scale,
                                 req->scale, req->scale,
                                 entry->width, entry->height,
                                 req->pixels[row * req->width + col]);
                }
            }

            entry->y_offset = req->y + (req->height * req->scale);
            entry->x_offset = DEFAULT_HORIZONTAL_PADDING;

            if (copy_pixels_to_screen(entry) == STATUS_ERROR) {
                return STATUS_ERROR;
            }
        }
    }

    return STATUS_OK;
}

static int gui_handle_request(request_table *req) {
    task_t *gui_task = task_get_by_pid(gui_task_pid);

    if (gui_task == NULL || req == NULL)
        return STATUS_ERROR;

    switch (req->request_type) {
    case WRITE:
        req->status        = (gui_draw_string(req->buf, req->caller_pid) == STATUS_OK) ? COMPLETE : FAILED;
        gui_task->priority = PRIORITY_NORMAL;
        scheduler_wake_task(req->caller_pid);
        return STATUS_OK;
    case CREATE:
        req->status        = (gui_create_window_entry(req->caller_pid, req->width, req->height, req->x, req->y) == STATUS_OK) ? COMPLETE : FAILED;
        gui_task->priority = PRIORITY_NORMAL;
        scheduler_wake_task(req->caller_pid);
        return STATUS_OK;
    case DELETE:
        __asm__ __volatile__("cli");
        req->status        = (gui_delete_window(req->caller_pid) == STATUS_OK) ? COMPLETE : FAILED;
        gui_task->priority = PRIORITY_NORMAL;
        return STATUS_OK;
    case PAINT_WINDOW:
        req->status        = (gui_paint_window_to_screen(req) == STATUS_OK) ? COMPLETE : FAILED;
        gui_task->priority = PRIORITY_NORMAL;
        scheduler_wake_task(req->caller_pid);
        return STATUS_OK;
    case FG_COLOR:
        req->status        = (gui_change_fg_color(req->caller_pid, req->color) == STATUS_OK) ? COMPLETE : FAILED;
        gui_task->priority = PRIORITY_NORMAL;
        scheduler_wake_task(req->caller_pid);
        return STATUS_OK;
    case BG_COLOR:
        req->status        = (gui_change_bg_color(req->caller_pid, req->color) == STATUS_OK) ? COMPLETE : FAILED;
        gui_task->priority = PRIORITY_NORMAL;
        scheduler_wake_task(req->caller_pid);
        return STATUS_OK;
    case DRAW:
        req->status        = (gui_draw_sprite(req) == STATUS_OK) ? COMPLETE : FAILED;
        gui_task->priority = PRIORITY_NORMAL;
        scheduler_wake_task(req->caller_pid);
        return STATUS_OK;
    default:
        req->status = FAILED;
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

static void gui_recovery() {
    DEBUG_GUI_TASK("[GUI_TASK][RECOVERY]: activated %d times\n", hail_mary_act_count);
    hail_mary_act_count++;
    ledger_check_request(gui_task_pid);
    blankie_activate(gui_task_pid);
}

void gui_init(task_t *gui_task) {
    DEBUG_GUI_TASK("[GUI_TASK][INIT]: Initializing GUI\n");
    blankie_register(gui_task_pid, gui_task->context.eip, gui_task->kernel_stack);
    register_hail_mary_function(gui_task_pid, gui_recovery);
    fg_color = fb_pack_color(115, 145, 125);
    bg_color = fb_pack_color(70, 50, 100);

    DEBUG_GUI_TASK("[GUI_TASK][INIT]: Creating main screen\n");

    fb_clear((uint32_t *)fb.virt_addr, fb.width, fb.height, fg_color);

    /*uint32_t palette[] = {
        fb_pack_color(0, 0, 0),
        fb_pack_color(255, 0, 0),
    };

    int x     = 20;
    int y     = 20;
    int scale = 4;

    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 32; col++) {
            fb_fill_rect((uint32_t *)fb.virt_addr,
                         x + col * scale, y + row * scale,
                         scale, scale,
                         fb.width, fb.height,
                         sprite[row][col]);
        }
    }*/

    memset(compositor, 0, sizeof(compositor));

    DEBUG_GUI_TASK("[GUI_TASK][INIT]: Screen succesfully initialized\n");
}
