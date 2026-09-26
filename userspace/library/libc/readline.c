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
    uint32_t y;
    bool is_absolute;
} rdl_obj_t;

static void clear_input_line(rdl_obj_t *rdl_obj) {

    if (rdl_obj->is_absolute) {
        reset_region(rdl_obj->region_id);
        return;
    }

    for (uint32_t i = 0; i < rdl_obj->pos; i++) {
        print_to_region(rdl_obj->region_id, "\b");
    }
}

static inline void handle_enter(gfx_region_t *entry, rdl_obj_t *rdl_obj, char *buf) {

    if (rdl_obj->pos < (uint32_t)strlen(buf)) {
        rdl_obj->pos = strlen(buf) + 1;
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

static inline void handle_backspace(gfx_region_t *entry, rdl_obj_t *rdl_obj) {

    if (rdl_obj == NULL || rdl_obj->pos <= 0) {
        return;
    }

    rdl_obj->pos--;

    if (rdl_obj->is_absolute) {
        mark_cursor_position(rdl_obj->x, rdl_obj->y, entry->bg_color);
        rdl_obj->x -= FONT_WIDTH;
        print_at(rdl_obj->region_id, rdl_obj->x, rdl_obj->y, " ");
        return;
    }

    mark_cursor_position(entry->cursor_x, entry->cursor_y, entry->bg_color);
    print_to_region(rdl_obj->region_id, "\b");
}

static int handle_hist_key(gfx_region_t *entry, rdl_obj_t *rdl_obj, const char c, char *buf) {

    bool should_print = false;

    switch (c) {
    case KEY_UP: {

        const char *cmd = history_get_next();
        if (cmd != NULL) {
            strncpy(buf, cmd, rdl_obj->original_buf_len);
            rdl_obj->pos = strlen(buf);
            should_print = true;
        }
        break;
    }

    case KEY_DOWN: {
        const char *cmd = history_get_prev();
        if (cmd != NULL) {
            strncpy(buf, cmd, rdl_obj->original_buf_len);
            rdl_obj->pos = strlen(buf);
            should_print = true;
        }
        break;
    }

    default:
        return STATUS_ERROR;
    }

    if (should_print) {
        clear_input_line(rdl_obj);
        buf[rdl_obj->original_buf_len - 1] = '\0';
        rdl_obj->pos                       = strlen(buf);

        if (rdl_obj->is_absolute) {
            mark_cursor_position(rdl_obj->x, rdl_obj->y, entry->bg_color);
            print_at(rdl_obj->region_id, rdl_obj->x, rdl_obj->y, buf);
            rdl_obj->x += strlen(buf) * FONT_WIDTH;
        } else {
            mark_cursor_position(entry->cursor_x, entry->cursor_y, entry->bg_color);
            print_to_region(rdl_obj->region_id, buf);
        }
    }

    return STATUS_OK;
}

static int handle_arrow_key(gfx_region_t *entry, rdl_obj_t *rdl_obj, const char c) {

    if (c != KEY_LEFT && c != KEY_RIGHT) {
        return STATUS_ERROR;
    }

    if (rdl_obj->is_absolute && (entry->cursor_x < entry->border_width + entry->padding_x ||
                                 entry->cursor_x >= entry->width - entry->border_width - entry->padding_x)) {
        return STATUS_OK;
    }

    switch (c) {
    case KEY_LEFT:

        if (!rdl_obj->is_absolute && (entry->cursor_x - FONT_WIDTH < rdl_obj->x)) {
            break;
        }

        rdl_obj->pos--;

        if (rdl_obj->is_absolute) {
            mark_cursor_position(rdl_obj->x, rdl_obj->y, entry->bg_color);
            break;
        }

        mark_cursor_position(entry->cursor_x, entry->cursor_y, entry->bg_color);
        entry->cursor_x -= FONT_WIDTH;
        break;
    case KEY_RIGHT:
        if (rdl_obj->pos >= rdl_obj->current_buf_len) {
            break;
        }

        rdl_obj->pos--;

        if (rdl_obj->is_absolute) {
            mark_cursor_position(rdl_obj->x, rdl_obj->y, entry->bg_color);
            break;
        }

        mark_cursor_position(entry->cursor_x, entry->cursor_y, entry->bg_color);
        entry->cursor_x += FONT_WIDTH;
        break;
    }

    return STATUS_OK;
}

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

        if (handle_arrow_key(entry, rdl_obj, c) == STATUS_OK) {
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
    rdl_obj.y                = y;
    rdl_obj.is_absolute      = true;

    rdl_handle(&rdl_obj, buf);
}
