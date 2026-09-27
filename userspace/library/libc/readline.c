#include "readline.h"
#include "font.h"
#include "history.h"
#include "render.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "ui.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint32_t region_id;
    uint32_t pos;
    uint32_t original_buf_len;
    uint32_t current_buf_len;
    uint32_t x;
    uint32_t start_x;
    uint32_t y;
    bool is_absolute;
} rdl_obj_t;

/**
 * clear_input_line - to wipe from the current line completely.
 * @entry: region entry struct holding information about the region.
 * @rdl_obj: currently in use readline object.
 *
 * Description:
 * This function checks if current readline is positioned absolute. Absolute positions clear
 * the specific text area by overwriting with spaces starting from start_x. Relative rdl prints
 * backspaces length of the current buffer.
 *
 * Return: VOID.
 */
static void clear_input_line(gfx_region_t *entry, rdl_obj_t *rdl_obj) {

    if (rdl_obj->is_absolute) {
        mark_cursor_position(rdl_obj->x, rdl_obj->y, entry->bg_color);
        uint32_t len     = rdl_obj->current_buf_len > rdl_obj->pos ? rdl_obj->current_buf_len : rdl_obj->pos;
        uint32_t clear_x = rdl_obj->start_x;
        for (uint32_t i = 0; i < len; i++) {
            print_at(rdl_obj->region_id, clear_x, rdl_obj->y, " ");
            clear_x += FONT_WIDTH;
        }
        rdl_obj->x = rdl_obj->start_x;
        return;
    }

    mark_cursor_position(entry->cursor_x, entry->cursor_y, entry->bg_color);
    for (uint32_t i = 0; i < rdl_obj->pos; i++) {
        print_to_region(rdl_obj->region_id, "\b");
    }
}

/**
 * handle_enter - enter press logic.
 * @entry: region entry struct holding information about the region.
 * @rdl_obj: currently in use readline object.
 * @buf: buffer that hold the string to be added to history
 *
 * Description:
 * This function inserts a '\0' to the end of the buffer then adds it to history.
 * After that it checks if the rdl is absolute. If it is it hides the cursor line and returns.
 * On relative rdl it hides the cursor and prints one newline control char.
 *
 * Return: VOID.
 */
static inline void handle_enter(gfx_region_t *entry, rdl_obj_t *rdl_obj, char *buf) {

    if (rdl_obj->pos < (uint32_t)strlen(buf)) {
        rdl_obj->pos = strlen(buf);
    }

    buf[rdl_obj->pos] = '\0';
    history_add(buf);

    if (rdl_obj->is_absolute) {
        mark_cursor_position(rdl_obj->x, rdl_obj->y, entry->bg_color);
        return;
    }

    mark_cursor_position(entry->cursor_x, entry->cursor_y, entry->bg_color);
    print_to_region(rdl_obj->region_id, "\n");
}

/**
 * handle_backspace - backspace press logic.
 * @entry: region entry struct holding information about the region.
 * @rdl_obj: currently in use readline object.
 *
 * Description:
 * This function performs logic for the backspace press. It subtracts one from the rdl.pos. Then it
 * checks if the rdl is absolute. If it is it hides cursor marker, prints one space to
 * current x position (guarded by start_x), and subtracts one char width from rdl.x.
 *
 * If rdl is relative the function hides the cursor marker and prints one backspace.
 *
 * Return: VOID.
 */
static inline void handle_backspace(gfx_region_t *entry, rdl_obj_t *rdl_obj) {

    if (rdl_obj == NULL || rdl_obj->pos <= 0) {
        return;
    }

    rdl_obj->pos--;

    if (rdl_obj->is_absolute) {
        mark_cursor_position(rdl_obj->x, rdl_obj->y, entry->bg_color);
        if (rdl_obj->x > rdl_obj->start_x) {
            rdl_obj->x -= FONT_WIDTH;
        }
        print_at(rdl_obj->region_id, rdl_obj->x, rdl_obj->y, " ");
        return;
    }

    mark_cursor_position(entry->cursor_x, entry->cursor_y, entry->bg_color);
    print_to_region(rdl_obj->region_id, "\b");
}

/**
 * handle_hist_key - Handles arrow up and down logic.
 * @entry: region entry struct holding information about the region.
 * @rdl_obj: currently in use readline object.
 * @c: latest char.
 * @buf: buffer where to store potential command from history.
 *
 * Description:
 * Handles history navigation. For absolute rdl, rdl.x is reverted to start_x before printing.
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
static int handle_hist_key(gfx_region_t *entry, rdl_obj_t *rdl_obj, const char c, char *buf) {

    if (c != KEY_DOWN && c != KEY_UP) {
        return STATUS_ERROR;
    }

    const char *cmd = c == KEY_UP ? history_get_next() : history_get_prev();

    if (cmd != NULL) {
        strncpy(buf, cmd, rdl_obj->original_buf_len);
        clear_input_line(entry, rdl_obj);
        rdl_obj->pos = strlen(buf);

        if (rdl_obj->is_absolute) {
            rdl_obj->x = rdl_obj->start_x;
            mark_cursor_position(rdl_obj->x, rdl_obj->y, entry->bg_color);
            print_at(rdl_obj->region_id, rdl_obj->x, rdl_obj->y, buf);
            rdl_obj->x += rdl_obj->pos * FONT_WIDTH;
        } else {
            entry->cursor_x = rdl_obj->x;
            mark_cursor_position(entry->cursor_x, entry->cursor_y, entry->bg_color);
            print_to_region(rdl_obj->region_id, buf);
        }
    }

    return STATUS_OK;
}
/**
 * handle_nav_key - Handles arrow right and left logic.
 * @entry: region entry struct holding information about the region.
 * @rdl_obj: currently in use readline object.
 * @c: lates char.
 *
 * Description:
 * First the function check that the c is either arrow key left or right. If c was left we first
 * check if on relative rdl we can do anything. If we can we continue by subtracting one from rdl
 * buffer position index and then on absolute rdl hide hide the cursor marker exit. On relative rdl
 * we hide the marker and then move region entry cursor to left by one char. If c is right we do the
 * same as above but opposite
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
static int handle_nav_key(gfx_region_t *entry, rdl_obj_t *rdl_obj, const char c) {
    if (c != KEY_LEFT && c != KEY_RIGHT) {
        return STATUS_ERROR;
    }

    switch (c) {
    case KEY_LEFT:
        if (rdl_obj->pos == 0) {
            break;
        }

        if (!rdl_obj->is_absolute && (entry->cursor_x - FONT_WIDTH < rdl_obj->x)) {
            break;
        }

        rdl_obj->pos--;

        if (rdl_obj->is_absolute) {
            mark_cursor_position(rdl_obj->x, rdl_obj->y, entry->bg_color);
            rdl_obj->x -= FONT_WIDTH;
        } else {
            mark_cursor_position(entry->cursor_x, entry->cursor_y, entry->bg_color);
            entry->cursor_x -= FONT_WIDTH;
        }
        break;

    case KEY_RIGHT:
        if (rdl_obj->pos >= rdl_obj->current_buf_len) {
            break;
        }

        rdl_obj->pos++;

        if (rdl_obj->is_absolute) {
            mark_cursor_position(rdl_obj->x, rdl_obj->y, entry->bg_color);
            rdl_obj->x += FONT_WIDTH;
        } else {
            mark_cursor_position(entry->cursor_x, entry->cursor_y, entry->bg_color);
            entry->cursor_x += FONT_WIDTH;
        }
        break;
    }

    return STATUS_OK;
}

/**
 * rdl hanlde - Main logic loop of the readline function.
 * @rdl_obj: currently in use readline object.
 * @buf: buffer where chars will be stored.
 *
 * Description:
 * This function searches for the correct region entry so that its color data can be used or its
 * cursor positions modified. After it is found the function moves to core loop of the readline. It
 * first saves current buffer length to rdl_obj. Then it marks the cursors position with one pixel
 * width vertical line. After that it scans for userinput. Good to know it that kernel will block
 * the task using readline at this point as the task is waiting for input. Once user gives it the
 * given char is then inspected. If it was arrow key related char the loop begins from start as
 * there is nothing to print. If it was something else like backspace or enter the loop calls on
 * functions that handle those scenarios. On enter the buffer is returned to upstream caller. On
 * backspace the loops begins from start again. If the char was nothing of the above it lands on the
 * part of the functuons that prints it to screen
 *
 * Return: VOID
 */
static void rdl_handle(rdl_obj_t *rdl_obj, char *buf) {

    char c;

    gfx_region_t *entry = gfx_regions[rdl_obj->region_id];

    if (entry == NULL) {
        return;
    }

    while (1) {

        rdl_obj->current_buf_len = (uint32_t)strlen(buf);

        if (rdl_obj->is_absolute) {
            mark_cursor_position(rdl_obj->x, rdl_obj->y, entry->fg_color);
        } else {
            mark_cursor_position(entry->cursor_x, entry->cursor_y, entry->fg_color);
        }

        scan(&c);

        if (handle_hist_key(entry, rdl_obj, c, buf) == STATUS_OK) {
            continue;
        }

        if (handle_nav_key(entry, rdl_obj, c) == STATUS_OK) {
            continue;
        }

        if (c == '\n') {
            handle_enter(entry, rdl_obj, buf);
            return;
        }
        if (c == '\b') {
            handle_backspace(entry, rdl_obj);
            continue;
        }

        if (rdl_obj->pos < rdl_obj->original_buf_len - 1) {
            buf[rdl_obj->pos++] = c;
            char tmp[2]         = {c, '\0'};

            if (rdl_obj->is_absolute) {
                print_at(rdl_obj->region_id, rdl_obj->x, rdl_obj->y, tmp);
                rdl_obj->x += FONT_WIDTH;
            } else {
                print_to_region(rdl_obj->region_id, tmp);
            }
        }
    }
}

void readline_at_region(uint32_t region_id, char *buf, uint32_t original_buf_len) {
    rdl_obj_t rdl_obj;
    memset(&rdl_obj, 0, sizeof(rdl_obj));

    rdl_obj.region_id        = region_id;
    rdl_obj.original_buf_len = original_buf_len;
    rdl_obj.pos              = 0;
    rdl_obj.x                = get_region_cursor_x(region_id);
    rdl_obj.start_x          = rdl_obj.x;
    rdl_obj.y                = get_region_cursor_y(region_id);
    rdl_obj.is_absolute      = false;

    rdl_handle(&rdl_obj, buf);
}

void readline(char *buf, uint32_t original_buf_len) {
    rdl_obj_t rdl_obj;
    memset(&rdl_obj, 0, sizeof(rdl_obj));

    rdl_obj.region_id        = PRIMARY_VIEWPORT_ID;
    rdl_obj.original_buf_len = original_buf_len;
    rdl_obj.pos              = 0;
    rdl_obj.x                = get_region_cursor_x(PRIMARY_VIEWPORT_ID);
    rdl_obj.start_x          = rdl_obj.x;
    rdl_obj.y                = get_region_cursor_y(PRIMARY_VIEWPORT_ID);
    rdl_obj.is_absolute      = false;
    rdl_handle(&rdl_obj, buf);
}

void readline_at(uint32_t region_id, char *buf, uint32_t original_buf_len, uint32_t x, uint32_t y) {
    rdl_obj_t rdl_obj;
    memset(&rdl_obj, 0, sizeof(rdl_obj));

    rdl_obj.region_id        = region_id;
    rdl_obj.original_buf_len = original_buf_len;
    rdl_obj.pos              = 0;
    rdl_obj.x                = x;
    rdl_obj.start_x          = x;
    rdl_obj.y                = y;
    rdl_obj.is_absolute      = true;

    rdl_handle(&rdl_obj, buf);
}
